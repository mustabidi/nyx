/* Nyx GTK Integration Library
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

#include <glib/gi18n-lib.h>
#include <gst/gst.h>

#include "nyx-gtk-status-private.h"
#include "nyx-gtk-utils-private.h"

#define NORMAL_SPACING 16
#define ADAPT_SPACING 8

#define GST_CAT_DEFAULT nyx_gtk_status_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkStatus
{
  NyxGtkContainer parent;

  GtkWidget *status_box;
  GtkWidget *image;
  GtkWidget *title_label;
  GtkWidget *description_label;
};

#define parent_class nyx_gtk_status_parent_class
G_DEFINE_TYPE (NyxGtkStatus, nyx_gtk_status, NYX_GTK_TYPE_CONTAINER)

static void
adapt_cb (NyxGtkContainer *container, gboolean adapt,
    NyxGtkStatus *self)
{
  GST_DEBUG_OBJECT (self, "Adapted: %s", (adapt) ? "yes" : "no");

  gtk_box_set_spacing (GTK_BOX (self->status_box), (adapt) ? ADAPT_SPACING : NORMAL_SPACING);

  if (adapt) {
    gtk_widget_add_css_class (GTK_WIDGET (self), "adapted");
    gtk_widget_add_css_class (GTK_WIDGET (self->title_label), "title-2");
  } else {
    gtk_widget_remove_css_class (GTK_WIDGET (self), "adapted");
    gtk_widget_remove_css_class (GTK_WIDGET (self->title_label), "title-2");
  }
}

static void
_set_status (NyxGtkStatus *self, const gchar *icon_name,
    const gchar *title, const gchar *description)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (self->image), icon_name);
  gtk_label_set_label (GTK_LABEL (self->title_label), title);
  gtk_label_set_label (GTK_LABEL (self->description_label), description);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
}

void
nyx_gtk_status_set_error (NyxGtkStatus *self, const GError *error)
{
  GST_DEBUG_OBJECT (self, "Status set to \"error\"");
  _set_status (self, "dialog-warning-symbolic", _("Unplayable Content"), error->message);
}

void
nyx_gtk_status_set_missing_plugin (NyxGtkStatus *self, const gchar *name)
{
  gchar *description;

  GST_DEBUG_OBJECT (self, "Status set to \"missing-plugin\"");
  /* TRANSLATORS: Please do not try to translate "GStreamer" (it is a library name). */
  description = g_strdup_printf (_("Your GStreamer installation is missing a plugin: %s"), name);
  _set_status (self, "dialog-information-symbolic", _("Missing Plugin"), description);

  g_free (description);
}

void
nyx_gtk_status_clear (NyxGtkStatus *self)
{
  GST_DEBUG_OBJECT (self, "Status cleared");
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
nyx_gtk_status_init (NyxGtkStatus *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_box_set_spacing (GTK_BOX (self->status_box), NORMAL_SPACING);
}

static void
nyx_gtk_status_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), NYX_GTK_TYPE_STATUS);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_gtk_status_class_init (NyxGtkStatusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtkstatus", 0,
      "Nyx GTK Status");
  nyx_gtk_init_translations ();

  gobject_class->dispose = nyx_gtk_status_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
      NYX_GTK_RESOURCE_PREFIX "/ui/nyx-gtk-status.ui");

  gtk_widget_class_bind_template_child (widget_class, NyxGtkStatus, status_box);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkStatus, image);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkStatus, title_label);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkStatus, description_label);

  gtk_widget_class_bind_template_callback (widget_class, adapt_cb);

  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-status");
}
