/* Nyx Playback Library
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gst/gst.h>

#include "nyx-enhancer-director-private.h"
#include "../nyx-basic-functions.h"
#include "../nyx-cache-private.h"
#include "../nyx-enhancer-proxy-private.h"
#include "../nyx-extractable-private.h"
#include "../nyx-playlistable-private.h"
#include "../nyx-harvest-private.h"
#include "../nyx-media-item.h"
#include "../nyx-utils.h"
#include "../../shared/nyx-shared-utils-private.h"

#include "../nyx-functionalities-availability.h"

#if NYX_WITH_ENHANCERS_LOADER
#include "../nyx-enhancers-loader-private.h"
#endif

#define CLEANUP_INTERVAL 10800 // once every 3 hours

#define GST_CAT_DEFAULT nyx_enhancer_director_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxEnhancerDirector
{
  NyxThreadedObject parent;
};

#define parent_class nyx_enhancer_director_parent_class
G_DEFINE_TYPE (NyxEnhancerDirector, nyx_enhancer_director, NYX_TYPE_THREADED_OBJECT);

typedef struct
{
  NyxEnhancerDirector *director;
  GList *filtered_proxies;
  GUri *uri;
  GstBuffer *buffer;
  GCancellable *cancellable;
  GError **error;
} NyxEnhancerDirectorData;

static GMutex cleanup_lock;

static gpointer
nyx_enhancer_director_extract_in_thread (NyxEnhancerDirectorData *data)
{
  NyxEnhancerDirector *self = data->director;
  GList *el;
  NyxHarvest *harvest = NULL;
  gchar *uri_str;
  guint job_id;
  gboolean success = FALSE;

  GST_DEBUG_OBJECT (self, "Extraction start");

  /* Cancelled during thread switching */
  if (g_cancellable_is_cancelled (data->cancellable))
    return NULL;

  uri_str = g_uri_to_string (data->uri);
  job_id = g_str_hash (uri_str);

  GST_DEBUG_OBJECT (self, "Extracting URI: \"%s\", compatible enhancers: %u",
      uri_str, g_list_length (data->filtered_proxies));
  g_free (uri_str);

  for (el = data->filtered_proxies; el; el = g_list_next (el)) {
    NyxEnhancerProxy *proxy = NYX_ENHANCER_PROXY_CAST (el->data);
    NyxExtractable *extractable = NULL;
    GstStructure *config;

    /* Ensures that we do not start extraction of the same URI concurrently.
     * If given job is already running, blocks here until finished.
     * Afterwards we try to read extracted data from cache. */
    nyx_enhancer_proxy_await_job_start (proxy, job_id);

    /* Cancelled during waiting for usage access */
    if (g_cancellable_is_cancelled (data->cancellable)) {
      nyx_enhancer_proxy_remove_job (proxy, job_id);
      break;
    }

    harvest = nyx_harvest_new (); // fresh harvest for each iteration
    config = nyx_enhancer_proxy_make_current_config (proxy);

    if ((success = nyx_harvest_fill_from_cache (harvest, proxy, config, data->uri))
        || g_cancellable_is_cancelled (data->cancellable)) { // Check before extract
      gst_clear_structure (&config);
      nyx_enhancer_proxy_remove_job (proxy, job_id);
      break;
    }

#if NYX_WITH_ENHANCERS_LOADER
    extractable = NYX_EXTRACTABLE_CAST (
        nyx_enhancers_loader_create_enhancer (proxy, NYX_TYPE_EXTRACTABLE));
#endif

    if (extractable) {
      if (config)
        nyx_enhancer_proxy_apply_config_to_enhancer (proxy, config, (GObject *) extractable);

      success = nyx_extractable_extract (extractable, data->uri,
          harvest, data->cancellable, data->error);
      g_object_unref (extractable);

      /* We are done with extractable, but keep harvest and try to cache it */
      if (success) {
        if (!g_cancellable_is_cancelled (data->cancellable)) {
          nyx_harvest_set_enhancer_in_caps (harvest, proxy);
          nyx_harvest_export_to_cache (harvest, proxy, config, data->uri);
        }
        gst_clear_structure (&config);
        nyx_enhancer_proxy_remove_job (proxy, job_id);
        break;
      }
    }

    nyx_enhancer_proxy_remove_job (proxy, job_id);

    /* Cleanup to try again with next enhancer */
    g_clear_object (&harvest);
    gst_clear_structure (&config);
  }

  /* Cancelled during extraction or exporting to cache */
  if (g_cancellable_is_cancelled (data->cancellable))
    success = FALSE;

  if (!success) {
    gst_clear_object (&harvest);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      const gchar *err_msg = (g_cancellable_is_cancelled (data->cancellable))
          ? "Extraction was cancelled"
          : "Extraction failed";
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, "%s", err_msg);
    }
  }

  GST_DEBUG_OBJECT (self, "Extraction finish");

  return harvest;
}

static gpointer
nyx_enhancer_director_parse_in_thread (NyxEnhancerDirectorData *data)
{
  NyxEnhancerDirector *self = data->director;
  GstMemory *mem;
  GstMapInfo info;
  GBytes *bytes;
  GList *el;
  GListStore *playlist = NULL;
  gboolean success = FALSE;

  GST_DEBUG_OBJECT (self, "Parse start");

  /* Cancelled during thread switching */
  if (g_cancellable_is_cancelled (data->cancellable))
    return NULL;

  GST_DEBUG_OBJECT (self, "Enhancer proxies for buffer: %u",
      g_list_length (data->filtered_proxies));

  mem = gst_buffer_peek_memory (data->buffer, 0);
  if (!mem || !gst_memory_map (mem, &info, GST_MAP_READ)) {
    g_set_error (data->error, GST_RESOURCE_ERROR,
        GST_RESOURCE_ERROR_FAILED, "Could not read playlist buffer data");
    return NULL;
  }

  bytes = g_bytes_new_static (info.data, info.size);

  for (el = data->filtered_proxies; el; el = g_list_next (el)) {
    NyxEnhancerProxy *proxy = NYX_ENHANCER_PROXY_CAST (el->data);
    NyxPlaylistable *playlistable = NULL;

    if (g_cancellable_is_cancelled (data->cancellable)) // Check before loading enhancer
      break;

#if NYX_WITH_ENHANCERS_LOADER
    playlistable = NYX_PLAYLISTABLE_CAST (
        nyx_enhancers_loader_create_enhancer (proxy, NYX_TYPE_PLAYLISTABLE));
#endif

    if (playlistable) {
      GstStructure *config;

      if ((config = nyx_enhancer_proxy_make_current_config (proxy))) {
        nyx_enhancer_proxy_apply_config_to_enhancer (proxy, config, (GObject *) playlistable);
        gst_structure_free (config);
      }

      if (g_cancellable_is_cancelled (data->cancellable)) { // Check before parse
        g_object_unref (playlistable);
        break;
      }

      playlist = g_list_store_new (NYX_TYPE_MEDIA_ITEM); // fresh list store for each iteration

      success = nyx_playlistable_parse (playlistable, data->uri, bytes,
          playlist, data->cancellable, data->error);
      g_object_unref (playlistable);

      /* We are done with playlistable, but keep playlist */
      if (success)
        break;

      /* Cleanup to try again with next enhancer */
      g_clear_object (&playlist);
    }
  }

  /* Unref bytes, then unmap their data */
  g_bytes_unref (bytes);
  gst_memory_unmap (mem, &info);

  /* Cancelled during parsing */
  if (g_cancellable_is_cancelled (data->cancellable))
    success = FALSE;

  if (!success) {
    g_clear_object (&playlist);

    /* Ensure we have some error set on failure */
    if (*data->error == NULL) {
      const gchar *err_msg = (g_cancellable_is_cancelled (data->cancellable))
          ? "Playlist parsing was cancelled"
          : "Could not parse playlist";
      g_set_error (data->error, GST_RESOURCE_ERROR,
          GST_RESOURCE_ERROR_FAILED, "%s", err_msg);
    }
  }

  GST_DEBUG_OBJECT (self, "Parse finish");

  return playlist;
}

static inline void
_harvest_delete_if_expired (NyxEnhancerDirector *self,
    NyxEnhancerProxy *proxy, GFile *file, const gint64 epoch_now)
{
  GMappedFile *mapped_file;
  const gchar *data;
  gchar *filename;
  GError *error = NULL;
  gboolean delete = TRUE;

  filename = g_file_get_path (file);

  if ((mapped_file = nyx_cache_open (filename, &data, &error))) {
    /* Do not delete if versions match and not expired */
    if (g_strcmp0 (nyx_cache_read_string (&data),
        nyx_enhancer_proxy_get_version (proxy)) == 0
        && nyx_cache_read_int64 (&data) > epoch_now) {
      delete = FALSE;
    }
    g_mapped_file_unref (mapped_file);
  } else if (error) {
    if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
      GST_DEBUG_OBJECT (self, "No cached harvest file found");
    else
      GST_ERROR_OBJECT (self, "Could not read cached harvest file, reason: %s", error->message);

    g_clear_error (&error);
  }

  if (delete) {
    if (G_LIKELY (g_file_delete (file, NULL, &error))) {
      GST_TRACE_OBJECT (self, "Deleted cached harvest: \"%s\"", filename);
    } else {
      GST_ERROR_OBJECT (self, "Could not delete harvest: \"%s\", reason: %s",
          filename, GST_STR_NULL (error->message));
      g_error_free (error);
    }
  }

  g_free (filename);
}

static inline void
_cache_proxy_harvests_cleanup (NyxEnhancerDirector *self,
    NyxEnhancerProxy *proxy, const gint64 epoch_now)
{
  GFile *dir;
  GFileEnumerator *dir_enum;
  GError *error = NULL;

  dir = g_file_new_build_filename (g_get_user_cache_dir (), NYX_API_NAME,
      "enhancers", nyx_enhancer_proxy_get_module_name (proxy),
      "harvests", NULL);

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GFile *child = NULL;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          &child, NULL, &error) || !info)
        break;

      if (G_LIKELY (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
          && g_str_has_suffix (g_file_info_get_name (info), ".bin")))
        _harvest_delete_if_expired (self, proxy, child, epoch_now);
    }

    g_object_unref (dir_enum);
  }

  if (error) {
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_FOUND) {
      gchar *path = g_file_get_path (dir);

      GST_ERROR_OBJECT (self, "Could not cleanup in dir: \"%s\", reason: %s",
          path, GST_STR_NULL (error->message));
      g_free (path);
    }

    g_error_free (error);
  }

  g_object_unref (dir);
}

static gboolean
_cache_cleanup_func (NyxEnhancerDirector *self)
{
  GMappedFile *mapped_file;
  GDateTime *date;
  GError *error = NULL;
  gchar *filename;
  const gchar *data;
  gint64 since_cleanup, epoch_now, epoch_last = 0;

  if (!g_mutex_trylock (&cleanup_lock)) {
    GST_LOG_OBJECT (self, "Cache cleanup is already running");
    return G_SOURCE_REMOVE;
  }

  date = g_date_time_new_now_utc ();
  epoch_now = g_date_time_to_unix (date);
  g_date_time_unref (date);

  filename = g_build_filename (g_get_user_cache_dir (), NYX_API_NAME,
      "enhancers", "cleanup.bin", NULL);

  if ((mapped_file = nyx_cache_open (filename, &data, &error))) {
    epoch_last = nyx_cache_read_int64 (&data);
    g_mapped_file_unref (mapped_file);
  } else if (error) {
    if (error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT)
      GST_DEBUG_OBJECT (self, "No cache cleanup file found");
    else
      GST_ERROR_OBJECT (self, "Could not read cache cleanup file, reason: %s", error->message);

    g_clear_error (&error);
  }

  since_cleanup = epoch_now - epoch_last;

  if (since_cleanup >= CLEANUP_INTERVAL) {
    NyxEnhancerProxyList *proxies;
    guint i, n_proxies;
    GByteArray *bytes;

    GST_TRACE_OBJECT (self, "Time for cache cleanup, last was %"
        NYX_TIME_FORMAT " ago", NYX_TIME_ARGS (since_cleanup));

    /* Start with writing to cache cleanup time,
     * so other directors can find it earlier */
    if ((bytes = nyx_cache_create ())) {
      nyx_cache_store_int64 (bytes, epoch_now);

      if (nyx_cache_write (filename, bytes, &error)) {
        GST_TRACE_OBJECT (self, "Written data to cache cleanup file, cleanup time: %"
            G_GINT64_FORMAT, epoch_now);
      } else if (error) {
        GST_ERROR_OBJECT (self, "Could not write cache cleanup data, reason: %s", error->message);
        g_clear_error (&error);
      }

      g_byte_array_free (bytes, TRUE);
    }

    /* Now do cleanup */
    proxies = nyx_get_global_enhancer_proxies ();
    n_proxies = nyx_enhancer_proxy_list_get_n_proxies (proxies);

    for (i = 0; i < n_proxies; ++i) {
      NyxEnhancerProxy *proxy = nyx_enhancer_proxy_list_peek_proxy (proxies, i);

      if (!nyx_enhancer_proxy_target_has_interface (proxy, NYX_TYPE_EXTRACTABLE))
        continue;

      _cache_proxy_harvests_cleanup (self, proxy, epoch_now);
    }
  } else {
    GST_TRACE_OBJECT (self, "No cache cleanup yet, last was %"
        NYX_TIME_FORMAT " ago", NYX_TIME_ARGS (since_cleanup));
  }

  g_mutex_unlock (&cleanup_lock);
  g_free (filename);

  return G_SOURCE_REMOVE;
}

/*
 * nyx_enhancer_director_new:
 *
 * Returns: (transfer full): a new #NyxEnhancerDirector instance.
 */
NyxEnhancerDirector *
nyx_enhancer_director_new (void)
{
  NyxEnhancerDirector *director;

  director = g_object_new (NYX_TYPE_ENHANCER_DIRECTOR, NULL);
  gst_object_ref_sink (director);

  return director;
}

NyxHarvest *
nyx_enhancer_director_extract (NyxEnhancerDirector *self,
    GList *filtered_proxies, GUri *uri,
    GCancellable *cancellable, GError **error)
{
  NyxEnhancerDirectorData *data = g_new (NyxEnhancerDirectorData, 1);
  GMainContext *context;
  NyxHarvest *harvest;

  data->director = self;
  data->filtered_proxies = filtered_proxies;
  data->uri = uri;
  data->buffer = NULL;
  data->cancellable = cancellable;
  data->error = error;

  context = nyx_threaded_object_get_context (NYX_THREADED_OBJECT_CAST (self));

  harvest = NYX_HARVEST_CAST (nyx_shared_utils_context_invoke_sync_full (context,
      (GThreadFunc) nyx_enhancer_director_extract_in_thread,
      data, (GDestroyNotify) g_free));

  /* Run cleanup async. Since context belongs to "self", do not ref it.
   * This ensures clean shutdown with thread stop function called. */
  if (!g_cancellable_is_cancelled (cancellable) && !nyx_cache_is_disabled ())
    g_main_context_invoke (context, (GSourceFunc) _cache_cleanup_func, self);

  return harvest;
}

GListStore *
nyx_enhancer_director_parse (NyxEnhancerDirector *self,
    GList *filtered_proxies, GUri *uri, GstBuffer *buffer,
    GCancellable *cancellable, GError **error)
{
  NyxEnhancerDirectorData *data = g_new (NyxEnhancerDirectorData, 1);
  GMainContext *context;
  GListStore *playlist;

  data->director = self;
  data->filtered_proxies = filtered_proxies;
  data->uri = uri;
  data->buffer = buffer;
  data->cancellable = cancellable;
  data->error = error;

  context = nyx_threaded_object_get_context (NYX_THREADED_OBJECT_CAST (self));

  playlist = (GListStore *) nyx_shared_utils_context_invoke_sync_full (context,
      (GThreadFunc) nyx_enhancer_director_parse_in_thread,
      data, (GDestroyNotify) g_free);

  return playlist;
}

static void
nyx_enhancer_director_thread_start (NyxThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread start");
}

static void
nyx_enhancer_director_thread_stop (NyxThreadedObject *threaded_object)
{
  GST_TRACE_OBJECT (threaded_object, "Enhancer director thread stop");
}

static void
nyx_enhancer_director_init (NyxEnhancerDirector *self)
{
}

static void
nyx_enhancer_director_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_enhancer_director_class_init (NyxEnhancerDirectorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxThreadedObjectClass *threaded_object = (NyxThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxenhancerdirector", 0,
      "Nyx Enhancer Director");

  gobject_class->finalize = nyx_enhancer_director_finalize;

  threaded_object->thread_start = nyx_enhancer_director_thread_start;
  threaded_object->thread_stop = nyx_enhancer_director_thread_stop;
}
