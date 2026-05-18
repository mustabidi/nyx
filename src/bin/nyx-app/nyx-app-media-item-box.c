/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <nyx-gtk/nyx-gtk.h>
#include "nyx-app-media-item-box.h"

struct _NyxAppMediaItemBox
{
  GtkBox parent;

  NyxMediaItem *media_item;
  guint position;

  GtkWidget *drag_image;
  GtkWidget *index_label;
  GtkWidget *title_label;
  GtkWidget *ext_label;
};

enum
{
  PROP_0,
  PROP_MEDIA_ITEM,
  PROP_POSITION,
  PROP_LAST
};

#define parent_class nyx_app_media_item_box_parent_class
G_DEFINE_TYPE (NyxAppMediaItemBox, nyx_app_media_item_box, GTK_TYPE_BOX);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static gchar *
get_media_item_extension (NyxMediaItem *item)
{
  const gchar *uri;
  GFile *file;
  gchar *basename;
  gchar *dot;
  gchar *ext = NULL;

  if (!item)
    return NULL;

  uri = nyx_media_item_get_uri (item);
  if (!uri)
    return NULL;

  file = g_file_new_for_uri (uri);
  basename = g_file_get_basename (file);
  g_object_unref (file);

  if (basename) {
    dot = strrchr (basename, '.');
    if (dot && dot != basename && *(dot + 1) != '\0') {
      ext = g_ascii_strdown (dot, -1);
    }
    g_free (basename);
  }

  return ext;
}

NyxMediaItem *
nyx_app_media_item_box_get_media_item (NyxAppMediaItemBox *self)
{
  return self->media_item;
}

static void
nyx_app_media_item_box_init (NyxAppMediaItemBox *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 6);

  self->drag_image = gtk_image_new_from_icon_name ("list-drag-handle-symbolic");
  gtk_widget_set_halign (self->drag_image, GTK_ALIGN_START);
  gtk_widget_set_valign (self->drag_image, GTK_ALIGN_CENTER);
  gtk_widget_set_can_target (self->drag_image, FALSE);
  gtk_box_append (GTK_BOX (self), self->drag_image);

  self->index_label = gtk_label_new ("");
  gtk_widget_set_halign (self->index_label, GTK_ALIGN_START);
  gtk_widget_set_valign (self->index_label, GTK_ALIGN_CENTER);
  gtk_widget_set_can_target (self->index_label, FALSE);
  gtk_widget_add_css_class (self->index_label, "dim-label");
  gtk_widget_add_css_class (self->index_label, "numeric");
  gtk_label_set_width_chars (GTK_LABEL (self->index_label), 3);
  gtk_label_set_xalign (GTK_LABEL (self->index_label), 0.0);
  gtk_box_append (GTK_BOX (self), self->index_label);

  self->title_label = g_object_new (g_type_from_name ("NyxGtkTitleLabel"), NULL);
  gtk_widget_set_halign (self->title_label, GTK_ALIGN_START);
  gtk_widget_set_valign (self->title_label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (self->title_label, TRUE);
  gtk_widget_set_can_target (self->title_label, FALSE);
  nyx_gtk_title_label_set_fallback_to_uri (NYX_GTK_TITLE_LABEL (self->title_label), TRUE);
  gtk_widget_add_css_class (self->title_label, "heading");
  gtk_box_append (GTK_BOX (self), self->title_label);

  self->ext_label = gtk_label_new ("");
  gtk_widget_set_halign (self->ext_label, GTK_ALIGN_END);
  gtk_widget_set_valign (self->ext_label, GTK_ALIGN_CENTER);
  gtk_widget_set_can_target (self->ext_label, FALSE);
  gtk_widget_add_css_class (self->ext_label, "dim-label");
  gtk_widget_add_css_class (self->ext_label, "extension");
  gtk_widget_set_margin_end (self->ext_label, 6);
  gtk_box_append (GTK_BOX (self), self->ext_label);
}

static void
nyx_app_media_item_box_finalize (GObject *object)
{
  NyxAppMediaItemBox *self = NYX_APP_MEDIA_ITEM_BOX_CAST (object);

  gst_clear_object (&self->media_item);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_app_media_item_box_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxAppMediaItemBox *self = NYX_APP_MEDIA_ITEM_BOX_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      g_value_set_object (value, nyx_app_media_item_box_get_media_item (self));
      break;
    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_app_media_item_box_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxAppMediaItemBox *self = NYX_APP_MEDIA_ITEM_BOX_CAST (object);

  switch (prop_id) {
    case PROP_MEDIA_ITEM:
      gst_object_replace ((GstObject **) &self->media_item, GST_OBJECT_CAST (g_value_get_object (value)));
      nyx_gtk_title_label_set_media_item (NYX_GTK_TITLE_LABEL (self->title_label), self->media_item);
      {
        gchar *ext = get_media_item_extension (self->media_item);
        gtk_label_set_text (GTK_LABEL (self->ext_label), ext ? ext : "");
        g_free (ext);
      }
      break;
    case PROP_POSITION:
      self->position = g_value_get_uint (value);
      {
        gchar *text = g_strdup_printf ("%u.", self->position + 1);
        gtk_label_set_text (GTK_LABEL (self->index_label), text);
        g_free (text);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_app_media_item_box_class_init (NyxAppMediaItemBoxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = nyx_app_media_item_box_get_property;
  gobject_class->set_property = nyx_app_media_item_box_set_property;
  gobject_class->finalize = nyx_app_media_item_box_finalize;

  param_specs[PROP_MEDIA_ITEM] = g_param_spec_object ("media-item",
      NULL, NULL, NYX_TYPE_MEDIA_ITEM,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION] = g_param_spec_uint ("position",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
