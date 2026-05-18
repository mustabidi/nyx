/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 * Copyright (C) 2025 Moon
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
#include <string.h>

#include "nyx-app-dir-scanner.h"

#define GST_CAT_DEFAULT nyx_app_dir_scanner_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define ENUM_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN

static const gchar * const MEDIA_EXTENSIONS[] = {
  /* Video */
  "mp4", "m4v", "mkv", "webm", "avi", "mov", "wmv", "flv",
  "ogv", "ogg", "ts", "m2ts", "mts", "vob", "3gp", "3g2",
  "rmvb", "rm", "divx", "xvid", "hevc", "h264", "h265", "f4v",
  /* Audio */
  "mp3", "flac", "aac", "m4a", "wav", "wma", "oga", "opus",
  "aiff", "aif", "alac", "wv", "ape", "mka", "ra", "au",
  "snd", "mid", "midi", "amr", "ac3", "dts", "eac3",
  NULL
};

static const gchar * const MEDIA_MIME_PREFIXES[] = {
  "audio/",
  "video/",
  "application/ogg",
  "application/x-matroska",
  "application/vnd.rn-realmedia",
  NULL
};

#define MAX_RECURSION_DEPTH 32

typedef struct
{
  GFile *root;
  gboolean recursive;
  GPtrArray *items;
  guint depth;
} ScanTask;

static void
scan_task_free (ScanTask *task)
{
  g_clear_object (&task->root);
  g_clear_pointer (&task->items, g_ptr_array_unref);
  g_free (task);
}

G_DEFINE_QUARK (nyx-app-dir-scanner-error-quark,
    nyx_app_dir_scanner_error)

static gboolean
is_media_mime_type (const gchar *content_type)
{
  guint i;

  if (!content_type || *content_type == '\0')
    return FALSE;

  for (i = 0; MEDIA_MIME_PREFIXES[i] != NULL; i++) {
    if (g_str_has_prefix (content_type, MEDIA_MIME_PREFIXES[i]))
      return TRUE;
  }

  return FALSE;
}

static gboolean
is_media_extension (const gchar *filename)
{
  const gchar *dot;
  gchar *ext_lower;
  guint i;
  gboolean result = FALSE;

  if (!filename)
    return FALSE;

  dot = strrchr (filename, '.');
  if (!dot || dot == filename || *(dot + 1) == '\0')
    return FALSE;

  ext_lower = g_ascii_strdown (dot + 1, -1);

  for (i = 0; MEDIA_EXTENSIONS[i] != NULL; i++) {
    if (strcmp (ext_lower, MEDIA_EXTENSIONS[i]) == 0) {
      result = TRUE;
      break;
    }
  }

  g_free (ext_lower);
  return result;
}

static gboolean
file_info_is_media (GFileInfo *info)
{
  const gchar *content_type;
  const gchar *name;

  if (g_file_info_get_is_hidden (info))
    return FALSE;

  if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
    content_type = g_file_info_get_content_type (info);
  } else if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE)) {
    content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
  } else {
    content_type = NULL;
  }

  if (content_type && *content_type != '\0')
    return is_media_mime_type (content_type);

  name = g_file_info_get_name (info);
  return is_media_extension (name);
}

static gint
_natural_compare (const gchar *s1, const gchar *s2)
{
  if (!s1 && !s2) return 0;
  if (!s1) return -1;
  if (!s2) return 1;

  while (*s1 && *s2) {
    if (g_ascii_isdigit (*s1) && g_ascii_isdigit (*s2)) {
      const gchar *p1 = s1;
      const gchar *p2 = s2;
      const gchar *end1;
      const gchar *end2;
      gsize len1;
      gsize len2;

      /* Skip leading zeros */
      while (*p1 == '0') p1++;
      while (*p2 == '0') p2++;

      /* Count length of digit sequences */
      end1 = p1;
      while (g_ascii_isdigit (*end1)) end1++;

      end2 = p2;
      while (g_ascii_isdigit (*end2)) end2++;

      len1 = end1 - p1;
      len2 = end2 - p2;

      if (len1 != len2) {
        return (len1 < len2) ? -1 : 1;
      }

      /* If length is same, compare digit-by-digit */
      while (p1 < end1) {
        if (*p1 != *p2) {
          return (*p1 < *p2) ? -1 : 1;
        }
        p1++;
        p2++;
      }

      s1 = end1;
      s2 = end2;
    } else {
      gunichar c1 = g_utf8_get_char (s1);
      gunichar c2 = g_utf8_get_char (s2);

      gunichar lc1 = g_unichar_tolower (c1);
      gunichar lc2 = g_unichar_tolower (c2);

      if (lc1 != lc2) {
        return (lc1 < lc2) ? -1 : 1;
      }

      s1 = g_utf8_next_char (s1);
      s2 = g_utf8_next_char (s2);
    }
  }

  if (*s1) return 1;
  if (*s2) return -1;
  return 0;
}

static gint
media_item_natural_compare (gconstpointer a, gconstpointer b)
{
  NyxMediaItem *item_a = *(NyxMediaItem **) a;
  NyxMediaItem *item_b = *(NyxMediaItem **) b;
  const gchar *uri_a, *uri_b;
  gchar *path_a = NULL, *path_b = NULL;
  gint result;

  uri_a = nyx_media_item_get_uri (item_a);
  uri_b = nyx_media_item_get_uri (item_b);

  {
    GFile *fa = g_file_new_for_uri (uri_a);
    GFile *fb = g_file_new_for_uri (uri_b);
    path_a = g_file_get_path (fa);
    path_b = g_file_get_path (fb);
    g_object_unref (fa);
    g_object_unref (fb);
  }

  result = _natural_compare (path_a ? path_a : uri_a, path_b ? path_b : uri_b);

  g_free (path_a);
  g_free (path_b);

  return result;
}

static gboolean
scan_directory_recursive (GFile *dir, GPtrArray *items,
    gboolean recursive, guint depth, GCancellable *cancellable, GError **error)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GError *local_error = NULL;

  if (depth > MAX_RECURSION_DEPTH) {
    GST_WARNING ("Maximum recursion depth %u reached, stopping descent", MAX_RECURSION_DEPTH);
    return TRUE;
  }

  enumerator = g_file_enumerate_children (dir, ENUM_ATTRIBUTES,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, &local_error);

  if (!enumerator) {
    g_propagate_error (error, local_error);
    return FALSE;
  }

  while (TRUE) {
    GFileType type;

    info = g_file_enumerator_next_file (enumerator, cancellable, &local_error);

    if (!info) {
      if (local_error) {
        g_propagate_error (error, local_error);
        g_object_unref (enumerator);
        return FALSE;
      }
      break;
    }

    type = g_file_info_get_file_type (info);

    if (type == G_FILE_TYPE_REGULAR) {
      if (file_info_is_media (info)) {
        GFile *child;
        gchar *uri;
        NyxMediaItem *item;
        const gchar *name = g_file_info_get_name (info);

        child = g_file_get_child (dir, name);
        uri = g_file_get_uri (child);
        item = nyx_media_item_new (uri);

        g_ptr_array_add (items, item);

        g_free (uri);
        g_object_unref (child);
      }
    } else if (type == G_FILE_TYPE_DIRECTORY && recursive) {
      const gchar *name = g_file_info_get_name (info);

      if (!g_file_info_get_is_hidden (info)) {
        GFile *subdir = g_file_get_child (dir, name);
        gboolean ok;

        ok = scan_directory_recursive (subdir, items, recursive,
            depth + 1, cancellable, &local_error);
        g_object_unref (subdir);

        if (!ok) {
          g_propagate_error (error, local_error);
          g_object_unref (info);
          g_object_unref (enumerator);
          return FALSE;
        }
      }
    }

    g_object_unref (info);
  }

  g_file_enumerator_close (enumerator, cancellable, NULL);
  g_object_unref (enumerator);

  return TRUE;
}

static void
dir_scanner_worker (GTask *task, gpointer source_object,
    gpointer task_data, GCancellable *cancellable)
{
  ScanTask *ctx = (ScanTask *) task_data;
  GError *error = NULL;
  gboolean ok;

  (void) source_object;

  ok = scan_directory_recursive (ctx->root, ctx->items,
      ctx->recursive, 0, cancellable, &error);

  if (!ok) {
    g_task_return_error (task, error);
    return;
  }

  if (ctx->items->len == 0) {
    g_task_return_new_error (task, NYX_APP_DIR_SCANNER_ERROR,
        NYX_APP_DIR_SCANNER_ERROR_EMPTY,
        _("No supported media files found in the selected folder."));
    return;
  }

  g_ptr_array_sort (ctx->items, media_item_natural_compare);

  g_task_return_pointer (task, g_steal_pointer (&ctx->items),
      (GDestroyNotify) g_ptr_array_unref);
}

typedef struct
{
  NyxAppDirScanCallback callback;
  gpointer user_data;
} CallbackData;

static void
dir_scanner_task_complete (GObject *source_object,
    GAsyncResult *result, gpointer user_data)
{
  CallbackData *cb_data = (CallbackData *) user_data;
  GPtrArray *items = NULL;
  GError *error = NULL;

  (void) source_object;

  items = g_task_propagate_pointer (G_TASK (result), &error);

  cb_data->callback (items, error, cb_data->user_data);

  g_clear_error (&error);
  g_free (cb_data);
}

void
nyx_app_dir_scanner_scan_async (GFile *directory,
    gboolean recursive, GCancellable *cancellable,
    NyxAppDirScanCallback callback, gpointer user_data)
{
  GTask *task;
  ScanTask *ctx;
  CallbackData *cb_data;

  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (callback != NULL);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxappdirscanner", 0,
      "Nyx App Dir Scanner");

  ctx = g_new0 (ScanTask, 1);
  ctx->root = g_object_ref (directory);
  ctx->recursive = recursive;
  ctx->depth = 0;
  ctx->items = g_ptr_array_new_with_free_func (g_object_unref);

  cb_data = g_new0 (CallbackData, 1);
  cb_data->callback = callback;
  cb_data->user_data = user_data;

  task = g_task_new (NULL, cancellable, dir_scanner_task_complete, cb_data);

  g_task_set_task_data (task, ctx, (GDestroyNotify) scan_task_free);
  g_task_set_name (task, "[nyx] dir-scanner");

  g_task_run_in_thread (task, dir_scanner_worker);

  g_object_unref (task);
}
