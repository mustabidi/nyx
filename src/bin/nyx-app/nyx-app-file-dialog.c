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

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "nyx-app-file-dialog.h"
#include "nyx-app-utils.h"
#include "nyx-app-window.h"

static inline void
_open_files_from_model (GtkApplication *gtk_app, GListModel *files_model)
{
  GFile **files = NULL;
  gint n_files = 0;

  if (nyx_app_utils_files_from_list_model (files_model, &files, &n_files)) {
    g_application_open (G_APPLICATION (gtk_app), files, n_files, "add-only");
    nyx_app_utils_files_free (files);
  }
}

static void
_open_files_cb (GtkFileDialog *dialog, GAsyncResult *result, GtkApplication *gtk_app)
{
  GError *error = NULL;
  GListModel *files_model = gtk_file_dialog_open_multiple_finish (dialog, result, &error);

  if (G_LIKELY (error == NULL)) {
    _open_files_from_model (gtk_app, files_model);
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }
  g_clear_object (&files_model);
}

static void
_open_subtitles_cb (GtkFileDialog *dialog, GAsyncResult *result, NyxMediaItem *item)
{
  GError *error = NULL;
  GFile *file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (G_LIKELY (error == NULL)) {
    gchar *suburi = g_file_get_uri (file);

    nyx_media_item_set_suburi (item, suburi);
    g_free (suburi);
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }
  g_clear_object (&file);
  gst_object_unref (item); // Borrowed reference
}

static void
_on_select_file_dir_finish (GFile *file, AdwActionRow *action_row, GError *error)
{
  if (G_LIKELY (error == NULL)) {
    gchar *path = g_file_get_path (file);

    adw_action_row_set_subtitle (action_row, path);
    g_free (path);
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }
  g_clear_object (&file);
  g_object_unref (action_row); // Borrowed reference
}

static void
_select_file_cb (GtkFileDialog *dialog, GAsyncResult *result, AdwActionRow *action_row)
{
  GError *error = NULL;
  GFile *file = gtk_file_dialog_open_finish (dialog, result, &error);

  _on_select_file_dir_finish (file, action_row, error);
}

static void
_select_dir_cb (GtkFileDialog *dialog, GAsyncResult *result, AdwActionRow *action_row)
{
  GError *error = NULL;
  GFile *file = gtk_file_dialog_select_folder_finish (dialog, result, &error);

  _on_select_file_dir_finish (file, action_row, error);
}

static void
_dialog_add_mime_types (GtkFileDialog *dialog, const gchar *filter_name,
    const gchar *const *mime_types)
{
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  GtkFileFilter *filter = gtk_file_filter_new ();
  guint i;

  /* XXX: Windows does not support mime-types file
   * filters, so use file extensions instead */
  for (i = 0; mime_types[i]; ++i) {
#ifndef G_OS_WIN32
    gtk_file_filter_add_mime_type (filter, mime_types[i]);
#else
    gtk_file_filter_add_suffix (filter, mime_types[i]);
#endif
  }

  gtk_file_filter_set_name (filter, filter_name);
  g_list_store_append (filters, filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  g_object_unref (filters);
  g_object_unref (filter);
}

void
nyx_app_file_dialog_open_files (GtkApplication *gtk_app)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  _dialog_add_mime_types (dialog, "Media Files",
#ifndef G_OS_WIN32
      nyx_app_utils_get_mime_types ());
#else
      nyx_app_utils_get_extensions ());
#endif

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Add Files");

  gtk_file_dialog_open_multiple (dialog, window, NULL,
      (GAsyncReadyCallback) _open_files_cb,
      gtk_app);

  g_object_unref (dialog);
}

void
nyx_app_file_dialog_open_subtitles (GtkApplication *gtk_app, NyxMediaItem *item)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  _dialog_add_mime_types (dialog, "Subtitles",
#ifndef G_OS_WIN32
      nyx_app_utils_get_subtitles_mime_types ());
#else
      nyx_app_utils_get_subtitles_extensions ());
#endif

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Open Subtitles");

  gtk_file_dialog_open (dialog, window, NULL,
      (GAsyncReadyCallback) _open_subtitles_cb,
      gst_object_ref (item));

  g_object_unref (dialog);
}

void
nyx_app_file_dialog_select_prefs_file (GtkApplication *gtk_app, AdwActionRow *action_row)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Select File");

  gtk_file_dialog_open (dialog, window, NULL,
      (GAsyncReadyCallback) _select_file_cb,
      g_object_ref (action_row));

  g_object_unref (dialog);
}

void
nyx_app_file_dialog_select_prefs_dir (GtkApplication *gtk_app, AdwActionRow *action_row)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, "Select Folder");

  gtk_file_dialog_select_folder (dialog, window, NULL,
      (GAsyncReadyCallback) _select_dir_cb,
      g_object_ref (action_row));

  g_object_unref (dialog);
}

typedef struct {
  GFile *file;
  NyxQueue *queue;
} OpenFolderData;

static void
_open_folder_dialog_response_cb (AdwAlertDialog *dialog,
    const gchar *response, gpointer user_data)
{
  OpenFolderData *data = (OpenFolderData *) user_data;

  if (g_strcmp0 (response, "replace") == 0) {
    nyx_queue_clear (data->queue);
    nyx_app_utils_handle_file_async (data->file, data->queue, TRUE, TRUE, NULL);
  } else if (g_strcmp0 (response, "append") == 0) {
    nyx_app_utils_handle_file_async (data->file, data->queue, FALSE, TRUE, NULL);
  }

  g_object_unref (data->file);
  g_object_unref (data->queue);
  g_free (data);
}

static void
_open_folder_cb (GtkFileDialog *dialog, GAsyncResult *result, GtkApplication *gtk_app)
{
  GError *error = NULL;
  GFile *file = gtk_file_dialog_select_folder_finish (dialog, result, &error);

  if (G_LIKELY (error == NULL)) {
    GtkWindow *window = gtk_application_get_active_window (gtk_app);

    if (window && NYX_APP_IS_WINDOW (window)) {
      NyxPlayer *player = nyx_app_window_get_player (NYX_APP_WINDOW (window));
      NyxQueue *queue = nyx_player_get_queue (player);

      if (nyx_queue_get_n_items (queue) == 0) {
        nyx_app_window_ensure_no_initial_state (NYX_APP_WINDOW (window));
        nyx_app_utils_handle_file_async (file, queue, TRUE, TRUE, NULL);
      } else {
        AdwDialog *alert;
        OpenFolderData *data;

        alert = adw_alert_dialog_new (_("Open Folder"),
            _("Do you want to replace the current playlist or append the folder contents to it?"));
        adw_alert_dialog_add_response (ADW_ALERT_DIALOG (alert), "replace", _("Replace"));
        adw_alert_dialog_add_response (ADW_ALERT_DIALOG (alert), "append", _("Append"));
        adw_alert_dialog_add_response (ADW_ALERT_DIALOG (alert), "cancel", _("Cancel"));
        adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "replace");
        adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");

        data = g_new0 (OpenFolderData, 1);
        data->file = g_object_ref (file);
        data->queue = g_object_ref (queue);

        g_signal_connect (alert, "response", G_CALLBACK (_open_folder_dialog_response_cb), data);
        adw_dialog_present (alert, GTK_WIDGET (window));
      }
    }
  } else {
    if (error->domain != GTK_DIALOG_ERROR || error->code != GTK_DIALOG_ERROR_DISMISSED) {
      g_printerr ("Error: %s\n",
          (error->message) ? error->message : "Could not open file dialog");
    }
    g_error_free (error);
  }

  g_clear_object (&file);
}

void
nyx_app_file_dialog_open_folder (GtkApplication *gtk_app)
{
  GtkWindow *window = gtk_application_get_active_window (gtk_app);
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  gtk_file_dialog_set_modal (dialog, TRUE);
  gtk_file_dialog_set_title (dialog, _("Select Folder"));

  gtk_file_dialog_select_folder (dialog, window, NULL,
      (GAsyncReadyCallback) _open_folder_cb,
      gtk_app);

  g_object_unref (dialog);
}
