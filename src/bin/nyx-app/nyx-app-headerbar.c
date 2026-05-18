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

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "nyx-app-headerbar.h"
#include "nyx-app-utils.h"

#define GST_CAT_DEFAULT nyx_app_headerbar_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxAppHeaderbar
{
  NyxGtkContainer parent;

  GtkWidget *queue_revealer;
  GtkWidget *previous_item_revealer;
  GtkWidget *next_item_revealer;
  GtkWidget *win_buttons_revealer;

  GtkDropTarget *drop_target;

  gboolean adapt;
};

#define parent_class nyx_app_headerbar_parent_class
G_DEFINE_TYPE (NyxAppHeaderbar, nyx_app_headerbar, NYX_GTK_TYPE_CONTAINER);

static void
_determine_win_buttons_reveal (NyxAppHeaderbar *self)
{
  gboolean queue_reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->win_buttons_revealer),
      (queue_reveal) ? !self->adapt : TRUE);
}

static void
container_adapt_cb (NyxGtkContainer *container, gboolean adapt,
    NyxAppHeaderbar *self)
{
  GST_DEBUG_OBJECT (self, "Width adapted: %s", (adapt) ? "yes" : "no");
  self->adapt = adapt;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->previous_item_revealer), !adapt);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->next_item_revealer), !adapt);

  _determine_win_buttons_reveal (self);
}

static void
queue_reveal_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, NyxAppHeaderbar *self)
{
  _determine_win_buttons_reveal (self);
}

static void
reveal_queue_button_clicked_cb (GtkButton *button, NyxAppHeaderbar *self)
{
  gboolean reveal;

  GST_INFO_OBJECT (self, "Reveal queue button clicked");

  reveal = gtk_revealer_get_reveal_child (GTK_REVEALER (self->queue_revealer));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->queue_revealer), !reveal);
}

static void
drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, NyxAppHeaderbar *self)
{
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (value && !nyx_app_utils_value_for_item_is_valid (value))
    gtk_drop_target_reject (drop_target);
}

static gboolean
drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, NyxAppHeaderbar *self)
{
  GFile **files = NULL;
  gint n_files = 0;
  gboolean success = FALSE;

  if (nyx_app_utils_files_from_value (value, &files, &n_files)) {
    NyxPlayer *player;

    if ((player = nyx_gtk_get_player_from_ancestor (GTK_WIDGET (self)))) {
      NyxQueue *queue = nyx_player_get_queue (player);
      gboolean start_playback = (nyx_queue_get_n_items (queue) == 0);

      nyx_app_utils_files_to_media_items_async (files, n_files, queue,
          start_playback, TRUE, NULL);

      success = TRUE;
    }

    nyx_app_utils_files_free (files);
  }

  return success;
}

static void
nyx_app_headerbar_init (NyxAppHeaderbar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_drop_target_set_gtypes (self->drop_target,
      (GType[3]) { GDK_TYPE_FILE_LIST, G_TYPE_FILE, G_TYPE_STRING }, 3);
}

static void
nyx_app_headerbar_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), NYX_APP_TYPE_HEADERBAR);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_app_headerbar_finalize (GObject *object)
{
  NyxAppHeaderbar *self = NYX_APP_HEADERBAR_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_app_headerbar_class_init (NyxAppHeaderbarClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxappheaderbar", 0,
      "Nyx App Headerbar");

  gobject_class->dispose = nyx_app_headerbar_dispose;
  gobject_class->finalize = nyx_app_headerbar_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
      NYX_APP_RESOURCE_PREFIX "/ui/nyx-app-headerbar.ui");

  gtk_widget_class_bind_template_child (widget_class, NyxAppHeaderbar, queue_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxAppHeaderbar, previous_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxAppHeaderbar, next_item_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxAppHeaderbar, win_buttons_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxAppHeaderbar, drop_target);

  gtk_widget_class_bind_template_callback (widget_class, container_adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, reveal_queue_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_reveal_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_cb);

  gtk_widget_class_set_css_name (widget_class, "nyx-app-headerbar");
}
