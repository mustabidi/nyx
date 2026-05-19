/* Nyx Application
 * Copyright (C) 2026 Moon
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
#include <gst/gst.h>

#include "nyx-app-sub-finder.h"
#include "nyx-app-utils.h"

#define GST_CAT_DEFAULT nyx_app_sub_finder_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define SUB_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN

#define MAX_RECURSION_DEPTH 3

static const gchar * const SUB_EXTENSIONS[] = {
  "srt", "vtt", "ass", "ssa", "sub", NULL
};

typedef struct
{
  GFile *file;
  gchar *uri;
  gint score;
} SubCandidate;

static SubCandidate *
sub_candidate_new (GFile *file, gint score)
{
  SubCandidate *candidate = g_new0 (SubCandidate, 1);
  candidate->file = g_object_ref (file);
  candidate->uri = g_file_get_uri (file);
  candidate->score = score;
  return candidate;
}

static void
sub_candidate_free (SubCandidate *candidate)
{
  g_clear_object (&candidate->file);
  g_free (candidate->uri);
  g_free (candidate);
}

typedef struct
{
  NyxMediaItem *item;
  GFile *parent_dir;
  gchar *movie_base_lower;
  GPtrArray *candidates;
} SubFinderTask;

static void
sub_finder_task_free (SubFinderTask *task)
{
  g_clear_object (&task->item);
  g_clear_object (&task->parent_dir);
  g_free (task->movie_base_lower);
  g_clear_pointer (&task->candidates, g_ptr_array_unref);
  g_free (task);
}

static gboolean
is_subtitle_extension (const gchar *filename)
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

  for (i = 0; SUB_EXTENSIONS[i] != NULL; i++) {
    if (strcmp (ext_lower, SUB_EXTENSIONS[i]) == 0) {
      result = TRUE;
      break;
    }
  }

  g_free (ext_lower);
  return result;
}

static gchar *
get_base_without_extension (const gchar *filename)
{
  const gchar *dot;
  if (!filename)
    return NULL;

  dot = strrchr (filename, '.');
  if (!dot || dot == filename)
    return g_strdup (filename);

  return g_strndup (filename, dot - filename);
}

static gint
calculate_score (const gchar *sub_name, const gchar *movie_base_lower, const gchar *parent_dir_name)
{
  gchar *sub_base = get_base_without_extension (sub_name);
  gchar *sub_base_lower = g_ascii_strdown (sub_base, -1);
  gint score = 0;

  if (strcmp (sub_base_lower, movie_base_lower) == 0) {
    // Exact match of basename
    score = 100;
  } else if (g_str_has_prefix (sub_base_lower, movie_base_lower)) {
    // Starts with movie basename (e.g. movie.eng, movie_fre, movie-en)
    gsize movie_len = strlen (movie_base_lower);
    gchar next_char = sub_base_lower[movie_len];
    if (next_char == '.' || next_char == '_' || next_char == '-' || next_char == ' ')
      score = 90;
    else
      score = 80;
  } else if (strstr (sub_base_lower, movie_base_lower) != NULL) {
    // Contains movie basename
    score = 70;
  } else if (strstr (movie_base_lower, sub_base_lower) != NULL) {
    // Movie basename contains subtitle name
    score = 50;
  } else if (parent_dir_name != NULL) {
    gchar *dir_lower = g_ascii_strdown (parent_dir_name, -1);
    if (strstr (dir_lower, "sub") != NULL || strstr (dir_lower, "subtitle") != NULL) {
      // Subtitle is inside a "Subs" or "Subtitles" directory and has a generic name
      if (strcmp (sub_base_lower, "english") == 0 || strcmp (sub_base_lower, "eng") == 0 ||
          strcmp (sub_base_lower, "en") == 0 || strcmp (sub_base_lower, "default") == 0 ||
          strcmp (sub_base_lower, "french") == 0 || strcmp (sub_base_lower, "spanish") == 0) {
        score = 40;
      }
    }
    g_free (dir_lower);
  }

  g_free (sub_base);
  g_free (sub_base_lower);
  return score;
}

static void
scan_directory_recursive (GFile *dir, SubFinderTask *task, guint depth, GCancellable *cancellable)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GError *error = NULL;

  if (depth > MAX_RECURSION_DEPTH)
    return;

  enumerator = g_file_enumerate_children (dir, SUB_ATTRIBUTES,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, &error);

  if (!enumerator) {
    if (error) {
      GST_WARNING ("Failed to enumerate directory: %s", error->message);
      g_error_free (error);
    }
    return;
  }

  while (TRUE) {
    GFileType type;
    const gchar *name;

    info = g_file_enumerator_next_file (enumerator, cancellable, &error);

    if (!info) {
      if (error) {
        GST_WARNING ("Error reading directory entry: %s", error->message);
        g_error_free (error);
      }
      break;
    }

    if (g_file_info_get_is_hidden (info)) {
      g_object_unref (info);
      continue;
    }

    name = g_file_info_get_name (info);
    type = g_file_info_get_file_type (info);

    if (type == G_FILE_TYPE_REGULAR) {
      if (is_subtitle_extension (name)) {
        GFile *child = g_file_get_child (dir, name);
        gchar *parent_name = g_file_get_basename (dir);
        gint score = calculate_score (name, task->movie_base_lower, parent_name);

        if (score >= 40) {
          SubCandidate *candidate = sub_candidate_new (child, score);
          g_ptr_array_add (task->candidates, candidate);
          GST_DEBUG ("Found subtitle candidate: %s with score: %d", name, score);
        }

        g_free (parent_name);
        g_object_unref (child);
      }
    } else if (type == G_FILE_TYPE_DIRECTORY) {
      GFile *subdir = g_file_get_child (dir, name);
      scan_directory_recursive (subdir, task, depth + 1, cancellable);
      g_object_unref (subdir);
    }

    g_object_unref (info);
  }

  g_file_enumerator_close (enumerator, cancellable, NULL);
  g_object_unref (enumerator);
}

static gint
sub_candidate_compare (gconstpointer a, gconstpointer b)
{
  SubCandidate *cand_a = *(SubCandidate **) a;
  SubCandidate *cand_b = *(SubCandidate **) b;

  // Higher score first
  return cand_b->score - cand_a->score;
}

static void
sub_finder_worker (GTask *task, gpointer source_object,
    gpointer task_data, GCancellable *cancellable)
{
  SubFinderTask *ctx = (SubFinderTask *) task_data;

  (void) source_object;

  GST_DEBUG ("Starting recursive subtitle search for movie base: %s", ctx->movie_base_lower);

  // Scan recursively starting from the movie's parent folder
  scan_directory_recursive (ctx->parent_dir, ctx, 0, cancellable);

  if (ctx->candidates->len > 0) {
    SubCandidate *best;
    // Sort descending by score
    g_ptr_array_sort (ctx->candidates, sub_candidate_compare);
    best = g_ptr_array_index (ctx->candidates, 0);

    GST_INFO ("Recursive subtitle search matched: %s (score: %d)", best->uri, best->score);
    g_task_return_pointer (task, g_strdup (best->uri), g_free);
  } else {
    GST_DEBUG ("No matching subtitles found recursively");
    g_task_return_pointer (task, NULL, NULL);
  }
}

static void
sub_finder_task_complete (GObject *source_object,
    GAsyncResult *result, gpointer user_data)
{
  SubFinderTask *task = (SubFinderTask *) user_data;
  gchar *best_suburi;

  (void) source_object;

  best_suburi = g_task_propagate_pointer (G_TASK (result), NULL);

  if (best_suburi) {
    GST_INFO ("Setting auto-resolved recursive subtitle: %s", best_suburi);
    nyx_media_item_set_suburi (task->item, best_suburi);
    g_free (best_suburi);
  }

  sub_finder_task_free (task);
}

void
nyx_app_sub_finder_find_for_item_async (NyxMediaItem *item, GCancellable *cancellable)
{
  const gchar *uri;
  GFile *media_file;
  gchar *movie_base;
  gchar *movie_name;
  SubFinderTask *task;
  GTask *gtask;

  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));

  uri = nyx_media_item_get_uri (item);
  if (!uri || !g_str_has_prefix (uri, "file://"))
    return; // Can only search local files

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxappsubfinder", 0,
      "Nyx App Sub Finder");

  media_file = g_file_new_for_uri (uri);
  movie_base = g_file_get_basename (media_file);
  movie_name = get_base_without_extension (movie_base);

  task = g_new0 (SubFinderTask, 1);
  task->item = g_object_ref (item);
  task->parent_dir = g_file_get_parent (media_file);
  task->movie_base_lower = g_ascii_strdown (movie_name, -1);
  task->candidates = g_ptr_array_new_with_free_func ((GDestroyNotify) sub_candidate_free);

  g_free (movie_name);
  g_free (movie_base);
  g_object_unref (media_file);

  if (!task->parent_dir) {
    sub_finder_task_free (task);
    return;
  }

  gtask = g_task_new (NULL, cancellable, sub_finder_task_complete, task);
  g_task_set_task_data (gtask, task, NULL);
  g_task_set_name (gtask, "[nyx] sub-finder");

  g_task_run_in_thread (gtask, sub_finder_worker);

  g_object_unref (gtask);
}
