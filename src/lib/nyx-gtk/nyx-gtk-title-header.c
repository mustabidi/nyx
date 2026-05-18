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

/**
 * NyxGtkTitleHeader:
 *
 * A header panel widget that displays current media title.
 *
 * #NyxGtkTitleHeader is a simple, ready to be used header widget that
 * displays current media title. It can be placed as-is as a [class@NyxGtk.Video]
 * overlay (either fading or not).
 *
 * If you need a further customized header, you can use [class@NyxGtk.TitleLabel]
 * which is used by this widget to build your own implementation instead.
 */

#include "config.h"

#include <gst/gst.h>

#include "nyx-gtk-title-header.h"
#include "nyx-gtk-title-label.h"

#define DEFAULT_FALLBACK_TO_URI FALSE

#define GST_CAT_DEFAULT nyx_gtk_title_header_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkTitleHeader
{
  NyxGtkLeadContainer parent;

  NyxGtkTitleLabel *label;
};

#define parent_class nyx_gtk_title_header_parent_class
G_DEFINE_TYPE (NyxGtkTitleHeader, nyx_gtk_title_header, NYX_GTK_TYPE_LEAD_CONTAINER)

enum
{
  PROP_0,
  PROP_CURRENT_TITLE,
  PROP_FALLBACK_TO_URI,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_label_current_title_changed_cb (NyxGtkTitleLabel *label G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkTitleHeader *self)
{
  /* Forward current title changed notify from internal label */
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_TITLE]);
}

/**
 * nyx_gtk_title_header_new:
 *
 * Creates a new #NyxGtkTitleHeader instance.
 *
 * Returns: a new title header #GtkWidget.
 */
GtkWidget *
nyx_gtk_title_header_new (void)
{
  return g_object_new (NYX_GTK_TYPE_TITLE_HEADER, NULL);
}

/**
 * nyx_gtk_title_header_get_current_title:
 * @header: a #NyxGtkTitleHeader
 *
 * Get currently displayed title by @header.
 *
 * Returns: (transfer none): text of title label.
 */
const gchar *
nyx_gtk_title_header_get_current_title (NyxGtkTitleHeader *self)
{
  g_return_val_if_fail (NYX_GTK_IS_TITLE_HEADER (self), NULL);

  return nyx_gtk_title_label_get_current_title (self->label);
}

/**
 * nyx_gtk_title_header_set_fallback_to_uri:
 * @header: a #NyxGtkTitleHeader
 * @enabled: whether enabled
 *
 * Set whether a [property@Nyx.MediaItem:uri] property should
 * be displayed as a header text when no other title could be determined.
 */
void
nyx_gtk_title_header_set_fallback_to_uri (NyxGtkTitleHeader *self, gboolean enabled)
{
  g_return_if_fail (NYX_GTK_IS_TITLE_HEADER (self));

  nyx_gtk_title_label_set_fallback_to_uri (self->label, enabled);
}

/**
 * nyx_gtk_title_header_get_fallback_to_uri:
 * @header: a #NyxGtkTitleHeader
 *
 * Get whether a [property@Nyx.MediaItem:uri] property is going
 * be displayed as a header text when no other title could be determined.
 *
 * Returns: %TRUE when item URI will be used as fallback, %FALSE otherwise.
 */
gboolean
nyx_gtk_title_header_get_fallback_to_uri (NyxGtkTitleHeader *self)
{
  g_return_val_if_fail (NYX_GTK_IS_TITLE_HEADER (self), FALSE);

  return nyx_gtk_title_label_get_fallback_to_uri (self->label);
}

static void
nyx_gtk_title_header_init (NyxGtkTitleHeader *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  nyx_gtk_title_label_set_fallback_to_uri (self->label, DEFAULT_FALLBACK_TO_URI);

  g_object_bind_property (self->label, "fallback-to-uri",
      self, "fallback-to-uri", G_BINDING_DEFAULT);
  g_signal_connect (self->label, "notify::current-title",
      G_CALLBACK (_label_current_title_changed_cb), self);
}

static void
nyx_gtk_title_header_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), NYX_GTK_TYPE_TITLE_HEADER);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_gtk_title_header_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxGtkTitleHeader *self = NYX_GTK_TITLE_HEADER_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_TITLE:
      g_value_set_string (value, nyx_gtk_title_header_get_current_title (self));
      break;
    case PROP_FALLBACK_TO_URI:
      g_value_set_boolean (value, nyx_gtk_title_header_get_fallback_to_uri (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_title_header_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxGtkTitleHeader *self = NYX_GTK_TITLE_HEADER_CAST (object);

  switch (prop_id) {
    case PROP_FALLBACK_TO_URI:
      nyx_gtk_title_header_set_fallback_to_uri (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_title_header_class_init (NyxGtkTitleHeaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtktitleheader", 0,
      "Nyx GTK Title Header");

  gobject_class->get_property = nyx_gtk_title_header_get_property;
  gobject_class->set_property = nyx_gtk_title_header_set_property;
  gobject_class->dispose = nyx_gtk_title_header_dispose;

  /**
   * NyxGtkTitleHeader:current-title:
   *
   * Currently displayed title.
   */
  param_specs[PROP_CURRENT_TITLE] = g_param_spec_string ("current-title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxGtkTitleHeader:fallback-to-uri:
   *
   * When title cannot be determined, show URI instead.
   */
  param_specs[PROP_FALLBACK_TO_URI] = g_param_spec_boolean ("fallback-to-uri",
      NULL, NULL, DEFAULT_FALLBACK_TO_URI,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_template_from_resource (widget_class,
      NYX_GTK_RESOURCE_PREFIX "/ui/nyx-gtk-title-header.ui");

  gtk_widget_class_bind_template_child (widget_class, NyxGtkTitleHeader, label);

  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-title-header");
}
