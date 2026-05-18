/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nyx-app-utils.h"
#include "nyx-app-media-item-box.h"
#include "nyx-app-dir-scanner.h"

#ifdef HAVE_GRAPHVIZ
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#ifdef HAVE_WIN_PROCESS_THREADS_API
#include <processthreadsapi.h>
#endif
#ifdef HAVE_WIN_TIME_API
#include <timeapi.h>
#endif
#endif

#define GST_CAT_DEFAULT nyx_app_utils_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
nyx_app_utils_debug_init (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxapputils", 0,
      "Nyx App Utils");
}

/* Windows specific functions */
#ifdef G_OS_WIN32

/*
 * nyx_app_utils_win_enforce_hi_res_clock:
 *
 * Enforce high resolution clock by explicitly disabling Windows
 * timer resolution power throttling. When disabled, system remembers
 * and honors any previous timer resolution request by the process.
 *
 * By default, Windows 11 may automatically ignore the timer
 * resolution requests in certain scenarios.
 */
void
nyx_app_utils_win_enforce_hi_res_clock (void)
{
#ifdef HAVE_WIN_PROCESS_THREADS_API
  PROCESS_POWER_THROTTLING_STATE PowerThrottling;
  gboolean success;

  RtlZeroMemory (&PowerThrottling, sizeof (PowerThrottling));

  PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
  PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
  PowerThrottling.StateMask = 0; // Always honor timer resolution requests

  success = (gboolean) SetProcessInformation(
      GetCurrentProcess (),
      ProcessPowerThrottling,
      &PowerThrottling,
      sizeof (PowerThrottling));

  /* Not an error. Older Windows does not have this functionality, but
   * also honor hi-res clock by default anyway, so do not print then. */
  GST_INFO ("Windows hi-res clock support is %senforced",
      (success) ? "" : "NOT ");
#endif
}

/*
 * nyx_app_utils_win_hi_res_clock_start:
 *
 * Start Windows high resolution clock which will improve
 * accuracy of various Windows timer APIs and precision
 * of #GstSystemClock during playback.
 *
 * On Windows 10 version 2004 (and older), this function affects
 * a global Windows setting. On any other (newer) version this
 * will only affect a single process.
 *
 * Returns: Timer resolution period value.
 */
guint
nyx_app_utils_win_hi_res_clock_start (void)
{
  guint resolution = 0;

#ifdef HAVE_WIN_TIME_API
  TIMECAPS time_caps;
  MMRESULT res;

  if ((res = timeGetDevCaps (&time_caps, sizeof (TIMECAPS))) != TIMERR_NOERROR) {
    GST_WARNING ("Could not query timer resolution, code: %u", res);
    return 0;
  }

  resolution = MIN (MAX (time_caps.wPeriodMin, 1), time_caps.wPeriodMax);

  if ((res = timeBeginPeriod (resolution)) != TIMERR_NOERROR) {
    GST_WARNING ("Could not request timer resolution, code: %u", res);
    return 0;
  }

  GST_INFO ("Started Windows hi-res clock, precision: %ums", resolution);
#endif

  return resolution;
}

/*
 * nyx_app_utils_win_hi_res_clock_stop:
 * @resolution: started resolution value (non-zero)
 *
 * Stop previously started Microsoft Windows high resolution clock.
 */
void
nyx_app_utils_win_hi_res_clock_stop (guint resolution)
{
#ifdef HAVE_WIN_TIME_API
  MMRESULT res;

  if ((res = timeEndPeriod (resolution)) == TIMERR_NOERROR)
    GST_INFO ("Stopped Windows hi-res clock");
  else
    GST_ERROR ("Could not stop hi-res clock, code: %u", res);
#endif
}

/* Extensions are used only on Windows */
const gchar *const *
nyx_app_utils_get_extensions (void)
{
  static const gchar *const all_extensions[] = {
    "avi", "claps", "m2ts", "mkv", "mov",
    "mp4", "webm", "wmv", NULL
  };

  return all_extensions;
}

const gchar *const *
nyx_app_utils_get_subtitles_extensions (void)
{
  static const gchar *const subs_extensions[] = {
    "srt", "vtt", NULL
  };

  return subs_extensions;
}
#endif // G_OS_WIN32

const gchar *const *
nyx_app_utils_get_mime_types (void)
{
  static const gchar *const all_mime_types[] = {
    "video/*",
    "audio/*",
    "application/claps",
    "application/x-subrip",
    "text/x-ssa",
    NULL
  };

  return all_mime_types;
}

const gchar *const *
nyx_app_utils_get_subtitles_mime_types (void)
{
  static const gchar *const subs_mime_types[] = {
    "application/x-subrip",
    "text/x-ssa",
    NULL
  };

  return subs_mime_types;
}

void
nyx_app_utils_parse_progression (NyxQueueProgressionMode mode,
    const gchar **icon, const gchar **label)
{
  const gchar *const icon_names[] = {
    "action-unavailable-symbolic",
    "media-playlist-consecutive-symbolic",
    "media-playlist-repeat-song-symbolic",
    "media-playlist-repeat-symbolic",
    "media-playlist-shuffle-symbolic",
    NULL
  };
  const gchar *const labels[] = {
    _("No progression"),
    _("Consecutive"),
    _("Repeat item"),
    _("Carousel"),
    _("Shuffle"),
    NULL
  };

  *icon = icon_names[mode];
  *label = labels[mode];
}

gboolean
nyx_app_utils_is_subtitles_file (GFile *file)
{
  GFileInfo *info;
  gboolean is_subs = FALSE;

  if ((info = g_file_query_info (file,
      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
      G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
      G_FILE_QUERY_INFO_NONE,
      NULL, NULL))) {
    const gchar *content_type = NULL;

    if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
      content_type = g_file_info_get_content_type (info);
    } else if (g_file_info_has_attribute (info,
        G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)) {
      content_type = g_file_info_get_attribute_string (info,
          G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    }

    is_subs = (content_type && g_strv_contains (
        nyx_app_utils_get_subtitles_mime_types (),
        content_type));

    g_object_unref (info);
  }

  return is_subs;
}

gboolean
nyx_app_utils_value_for_item_is_valid (const GValue *value)
{
  if (G_VALUE_HOLDS (value, GTK_TYPE_WIDGET))
    return NYX_APP_IS_MEDIA_ITEM_BOX (g_value_get_object (value));

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)
      || G_VALUE_HOLDS (value, G_TYPE_FILE))
    return TRUE;

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    return gst_uri_is_valid (g_value_get_string (value));

  return FALSE;
}

gboolean
nyx_app_utils_files_from_list_model (GListModel *files_model, GFile ***files, gint *n_files)
{
  guint i, len = g_list_model_get_n_items (files_model);

  if (G_UNLIKELY (len == 0 || len > G_MAXINT))
    return FALSE;

  *files = g_new (GFile *, len + 1);

  if (n_files)
    *n_files = (gint) len;

  for (i = 0; i < len; ++i) {
    (*files)[i] = g_list_model_get_item (files_model, i);
  }
  (*files)[i] = NULL;

  return TRUE;
}

gboolean
nyx_app_utils_files_from_slist (GSList *file_list, GFile ***files, gint *n_files)
{
  GSList *fl;
  guint len, i = 0;

  len = g_slist_length (file_list);

  if (G_UNLIKELY (len == 0 || len > G_MAXINT))
    return FALSE;

  *files = g_new (GFile *, len + 1);

  if (n_files)
    *n_files = (gint) len;

  for (fl = file_list; fl != NULL; fl = fl->next) {
    (*files)[i] = (GFile *) g_object_ref (fl->data);
    i++;
  }
  (*files)[i] = NULL;

  return TRUE;
}

gboolean
nyx_app_utils_files_from_string (const gchar *string, GFile ***files, gint *n_files)
{
  GSList *slist = NULL;
  gchar **uris = g_strsplit (string, "\n", 0);
  guint i;
  gboolean success;

  for (i = 0; uris[i]; ++i) {
    const gchar *uri = uris[i];

    if (!gst_uri_is_valid (uri))
      continue;

    slist = g_slist_append (slist, g_file_new_for_uri (uri));
  }

  g_strfreev (uris);

  if (!slist)
    return FALSE;

  success = nyx_app_utils_files_from_slist (slist, files, n_files);
  g_slist_free_full (slist, g_object_unref);

  return success;
}

gboolean
nyx_app_utils_files_from_command_line (GApplicationCommandLine *cmd_line, GFile ***files, gint *n_files)
{
  GSList *slist = NULL;
  gchar **argv;
  gint i, argc = 0;
  gboolean success;

  argv = g_application_command_line_get_arguments (cmd_line, &argc);

  for (i = 1; i < argc; ++i)
    slist = g_slist_append (slist, g_application_command_line_create_file_for_arg (cmd_line, argv[i]));

  g_strfreev (argv);

  if (!slist)
    return FALSE;

  success = nyx_app_utils_files_from_slist (slist, files, n_files);
  g_slist_free_full (slist, g_object_unref);

  return success;
}

static inline gboolean
_files_from_file (GFile *file, GFile ***files, gint *n_files)
{
  *files = g_new (GFile *, 2);

  (*files)[0] = g_object_ref (file);
  (*files)[1] = NULL;

  if (n_files)
    *n_files = 1;

  return TRUE;
}

gboolean
nyx_app_utils_files_from_value (const GValue *value, GFile ***files, gint *n_files)
{
  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST)) {
    return nyx_app_utils_files_from_slist (
        (GSList *) g_value_get_boxed (value), files, n_files);
  } else if (G_VALUE_HOLDS (value, G_TYPE_FILE)) {
    return _files_from_file (
        (GFile *) g_value_get_object (value), files, n_files);
  } else if (G_VALUE_HOLDS (value, G_TYPE_STRING)) {
    return nyx_app_utils_files_from_string (
        g_value_get_string (value), files, n_files);
  }

  return FALSE;
}

void
nyx_app_utils_files_free (GFile **files)
{
  gint i;

  for (i = 0; files[i]; ++i)
    g_object_unref (files[i]);

  g_free (files);
}

static inline gboolean
_parse_feature_name (gchar *str, const gchar **feature_name)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (str[0] == '\0')
    return FALSE;

  *feature_name = str;
  return TRUE;
}

static inline gboolean
_parse_feature_rank (gchar *str, GstRank *rank)
{
  if (!str)
    return FALSE;

  g_strstrip (str);

  if (str[0] == '\0')
    return FALSE;

  if (g_ascii_isdigit (str[0])) {
    gulong l;
    gchar *endptr;

    l = strtoul (str, &endptr, 10);
    if (endptr > str && endptr[0] == 0) {
      *rank = (GstRank) l;
    } else {
      return FALSE;
    }
  } else if (g_ascii_strcasecmp (str, "NONE") == 0) {
    *rank = GST_RANK_NONE;
  } else if (g_ascii_strcasecmp (str, "MARGINAL") == 0) {
    *rank = GST_RANK_MARGINAL;
  } else if (g_ascii_strcasecmp (str, "SECONDARY") == 0) {
    *rank = GST_RANK_SECONDARY;
  } else if (g_ascii_strcasecmp (str, "PRIMARY") == 0) {
    *rank = GST_RANK_PRIMARY;
  } else if (g_ascii_strcasecmp (str, "MAX") == 0) {
    *rank = (GstRank) G_MAXINT;
  } else {
    return FALSE;
  }

  return TRUE;
}

void
nyx_app_utils_iterate_plugin_feature_ranks (GSettings *settings,
    NyxAppUtilsIterRanks callback, gpointer user_data)
{
  gchar **split, **walk, *stored_overrides;
  const gchar *env_overrides;
  gboolean from_env = FALSE;

  stored_overrides = g_settings_get_string (settings, "plugin-feature-ranks");
  env_overrides = g_getenv ("GST_PLUGIN_FEATURE_RANK");

  /* Iterate from GSettings, then from ENV */
parse_overrides:
  split = g_strsplit ((from_env) ? env_overrides : stored_overrides, ",", 0);

  for (walk = split; *walk; walk++) {
    gchar **values;

    if (!strchr (*walk, ':'))
      continue;

    values = g_strsplit (*walk, ":", 2);

    if (g_strv_length (values) == 2) {
      GstRank rank;
      const gchar *feature_name;

      if (_parse_feature_name (values[0], &feature_name)
          && _parse_feature_rank (values[1], &rank))
        callback (feature_name, rank, from_env, user_data);
    }

    g_strfreev (values);
  }

  g_strfreev (split);

  if (!from_env && env_overrides) {
    from_env = TRUE;
    goto parse_overrides;
  }

  g_free (stored_overrides);
}

GstElement *
nyx_app_utils_make_element (const gchar *string)
{
  gchar *char_loc;

  if (strcmp (string, "none") == 0)
    return NULL;

  char_loc = strchr (string, ' ');

  if (char_loc) {
    GstElement *element;
    GError *error = NULL;

    element = gst_parse_bin_from_description (string, TRUE, &error);
    if (error) {
      GST_ERROR ("Bin parse error: \"%s\", reason: %s", string, error->message);
      g_error_free (error);
    }

    return element;
  }

  return gst_element_factory_make (string, NULL);
}

/*
 * _get_tmp_dir:
 * @subdir: (nullable): an optional subdirectory
 *
 * Returns: (transfer full): a newly constructed #GFile
 */
static inline GFile *
_get_tmp_dir (const gchar *subdir)
{
  /* XXX: System tmp directory does not work within containers such as Flatpak
   * for our usage with file launcher, so make our own temp in app data dir */
  return g_file_new_build_filename (
      g_get_user_data_dir (), NYX_APP_ID, "tmp", subdir, NULL);
}

#ifdef HAVE_GRAPHVIZ
static GFile *
_create_tmp_subdir (const gchar *subdir, GCancellable *cancellable, GError **error)
{
  GFile *tmp_dir;
  GError *my_error = NULL;

  tmp_dir = _get_tmp_dir (subdir);

  if (!g_file_make_directory_with_parents (tmp_dir, cancellable, &my_error)) {
    if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_EXISTS) {
      *error = g_error_copy (my_error);
      g_clear_object (&tmp_dir); // return NULL
    }
    g_error_free (my_error);
  }

  return tmp_dir;
}
#endif

static void
_create_pipeline_svg_file_in_thread (GTask *task, GObject *source G_GNUC_UNUSED,
    NyxPlayer *player, GCancellable *cancellable)
{
  GFile *tmp_file = NULL;
  GError *error = NULL;

#ifdef HAVE_GRAPHVIZ
  GFile *tmp_subdir;
  Agraph_t *graph;
  GVC_t *gvc;
  gchar *path, *template = NULL, *dot_data = NULL, *img_data = NULL;
  gint fd;
  gsize size = 0;

  if (!(tmp_subdir = _create_tmp_subdir ("pipelines", cancellable, &error)))
    goto finish;

  path = g_file_get_path (tmp_subdir);
  g_object_unref (tmp_subdir);

  template = g_build_filename (path, "pipeline-XXXXXX.svg", NULL);
  g_free (path);

  fd = g_mkstemp (template); // Modifies template to actual filename

  if (G_UNLIKELY (fd == -1)) {
    g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Could not open temp file for writing");
    goto finish;
  }

  dot_data = nyx_player_make_pipeline_graph (player, GST_DEBUG_GRAPH_SHOW_ALL);

  if (g_cancellable_is_cancelled (cancellable))
    goto close_and_finish;

  graph = agmemread (dot_data);

  gvc = gvContext ();
  gvLayout (gvc, graph, "dot");

#ifdef HAVE_GVC_13
  gvRenderData (gvc, graph, "svg", &img_data, &size);
#else
  {
    guint tmp_size = 0; // Temporary uint to satisfy older API
    gvRenderData (gvc, graph, "svg", &img_data, &tmp_size);
    size = tmp_size;
  }
#endif

  agclose (graph);
  gvFreeContext (gvc);

  if (g_cancellable_is_cancelled (cancellable))
    goto close_and_finish;

  if (write (fd, img_data, size) != -1) {
    tmp_file = g_file_new_for_path (template);
  } else {
    g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
        "Could not write data to temp file");
  }

close_and_finish:
  /* Always close the file IO */
  if (G_UNLIKELY (close (fd) == -1))
    GST_ERROR ("Could not close temp file!");

finish:
  g_free (template);
  g_free (dot_data);
  g_free (img_data);
#else
  g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
      "Cannot create graph file when compiled without Graphviz");
#endif

  if (tmp_file)
    g_task_return_pointer (task, tmp_file, (GDestroyNotify) g_object_unref);
  else
    g_task_return_error (task, error);
}

void
nyx_app_utils_create_pipeline_svg_file_async (NyxPlayer *player,
    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, gst_object_ref (player), (GDestroyNotify) gst_object_unref);
  g_task_run_in_thread (task, (GTaskThreadFunc) _create_pipeline_svg_file_in_thread);

  g_object_unref (task);
}

static gboolean
_delete_dir_recursive (GFile *dir, GError **error)
{
  GFileEnumerator *dir_enum;

  if ((dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error))) {
    while (TRUE) {
      GFileInfo *info = NULL;
      GFile *child = NULL;

      if (!g_file_enumerator_iterate (dir_enum, &info,
          &child, NULL, error) || !info)
        break;

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
        if (!_delete_dir_recursive (child, error))
          break;
      } else if (!g_file_delete (child, NULL, error)) {
        break;
      }
    }

    g_object_unref (dir_enum);
  }

  if (*error != NULL)
    return FALSE;

  return g_file_delete (dir, NULL, error);
}

void
nyx_app_utils_delete_tmp_dir (void)
{
  GFile *tmp_dir = _get_tmp_dir (NULL);
  GError *error = NULL;

  if (!_delete_dir_recursive (tmp_dir, &error)) {
    if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_NOT_FOUND) {
      GST_ERROR ("Could not remove temp dir, reason: %s",
          GST_STR_NULL (error->message));
    }
    g_error_free (error);
  }

  g_object_unref (tmp_dir);
}

typedef struct
{
  NyxQueue *queue;
  gboolean start_playback;
} DirScanContext;

static DirScanContext *
dir_scan_context_new (NyxQueue *queue, gboolean start_playback)
{
  DirScanContext *ctx = g_new0 (DirScanContext, 1);
  ctx->queue = g_object_ref (queue);
  ctx->start_playback = start_playback;
  return ctx;
}

static void
dir_scan_context_free (DirScanContext *ctx)
{
  g_clear_object (&ctx->queue);
  g_free (ctx);
}

static void
on_dir_scan_complete (GPtrArray *items, GError *error, gpointer user_data)
{
  DirScanContext *ctx = (DirScanContext *) user_data;
  guint i;

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      GST_WARNING ("Directory scan failed: %s", error->message);
    dir_scan_context_free (ctx);
    return;
  }

  if (!items || items->len == 0) {
    GST_DEBUG ("Directory scan returned no media items");
    dir_scan_context_free (ctx);
    return;
  }

  for (i = 0; i < items->len; i++) {
    NyxMediaItem *item = g_ptr_array_index (items, i);
    nyx_queue_add_item (ctx->queue, item);
  }

  g_ptr_array_unref (items);
  dir_scan_context_free (ctx);
}

void
nyx_app_utils_handle_file_async (GFile *file,
    NyxQueue *queue, gboolean start_playback,
    gboolean recursive, GCancellable *cancellable)
{
  GFileType type;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (NYX_IS_QUEUE (queue));

  type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, cancellable);

  if (type == G_FILE_TYPE_DIRECTORY) {
    DirScanContext *ctx;

    GST_DEBUG ("'%s' is a directory — initiating async scan",
        g_file_peek_path (file));

    ctx = dir_scan_context_new (queue, start_playback);

    nyx_app_dir_scanner_scan_async (file, recursive, cancellable,
        on_dir_scan_complete, ctx);
  } else if (type == G_FILE_TYPE_REGULAR ||
      type == G_FILE_TYPE_UNKNOWN ||
      type == G_FILE_TYPE_SYMBOLIC_LINK) {
    NyxMediaItem *item = nyx_media_item_new_from_file (file);

    GST_DEBUG ("Adding media item with URI: %s",
        nyx_media_item_get_uri (item));
    nyx_queue_add_item (queue, item);

    gst_object_unref (item);
  } else {
    GST_WARNING ("Unsupported file type %d for '%s'",
        type, g_file_peek_path (file));
  }
}

void
nyx_app_utils_files_to_media_items_async (GFile **files,
    gint n_files, NyxQueue *queue, gboolean start_playback,
    gboolean recursive, GCancellable *cancellable)
{
  gint i;

  g_return_if_fail (files != NULL);
  g_return_if_fail (n_files > 0);
  g_return_if_fail (NYX_IS_QUEUE (queue));

  for (i = 0; i < n_files; i++) {
    gboolean this_starts = start_playback && (i == 0);
    nyx_app_utils_handle_file_async (files[i], queue,
        this_starts, recursive, cancellable);
  }
}
