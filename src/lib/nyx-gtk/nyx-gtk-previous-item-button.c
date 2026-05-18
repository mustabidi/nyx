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
 * NyxGtkPreviousItemButton:
 *
 * A #GtkButton for selecting previous queue item.
 */

#include <nyx/nyx.h>

#include "nyx-gtk-previous-item-button.h"
#include "nyx-gtk-utils.h"

#define GST_CAT_DEFAULT nyx_gtk_previous_item_button_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkPreviousItemButton
{
  GtkButton parent;

  GBinding *n_items_binding;
  GBinding *current_index_binding;
};

#define parent_class nyx_gtk_previous_item_button_parent_class
G_DEFINE_TYPE (NyxGtkPreviousItemButton, nyx_gtk_previous_item_button, GTK_TYPE_BUTTON)

static gboolean
_transform_sensitive_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, NyxGtkPreviousItemButton *self)
{
  NyxQueue *queue = NYX_QUEUE_CAST (g_binding_dup_source (binding));
  guint current_index;
  gboolean can_skip;

  if (G_UNLIKELY (queue == NULL))
    return FALSE;

  current_index = nyx_queue_get_current_index (queue);

  can_skip = (current_index != NYX_QUEUE_INVALID_POSITION
      && current_index > 0);
  gst_object_unref (queue);

  g_value_set_boolean (to_value, can_skip);
  GST_DEBUG_OBJECT (self, "Set sensitive: %s", (can_skip) ? "yes" : "no");

  return TRUE;
}

/**
 * nyx_gtk_previous_item_button_new:
 *
 * Creates a new #NyxGtkPreviousItemButton to play previous #NyxMediaItem.
 *
 * Returns: a new previous item button #GtkWidget.
 */
GtkWidget *
nyx_gtk_previous_item_button_new (void)
{
  return g_object_new (NYX_GTK_TYPE_PREVIOUS_ITEM_BUTTON, NULL);
}

static void
nyx_gtk_previous_item_button_init (NyxGtkPreviousItemButton *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
  gtk_button_set_icon_name (GTK_BUTTON (self), "media-skip-backward-symbolic");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self), "av.previous-item");
}

static void
nyx_gtk_previous_item_button_map (GtkWidget *widget)
{
  NyxGtkPreviousItemButton *self = NYX_GTK_PREVIOUS_ITEM_BUTTON_CAST (widget);
  NyxPlayer *player;

  if ((player = nyx_gtk_get_player_from_ancestor (widget))) {
    NyxQueue *queue = nyx_player_get_queue (player);

    self->n_items_binding = g_object_bind_property_full (queue, "n-items",
        self, "sensitive", G_BINDING_DEFAULT, // Since we sync below, no need to do it twice
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
    self->current_index_binding = g_object_bind_property_full (queue, "current-index",
        self, "sensitive", G_BINDING_SYNC_CREATE,
        (GBindingTransformFunc) _transform_sensitive_func,
        NULL, self, NULL);
  }

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
nyx_gtk_previous_item_button_unmap (GtkWidget *widget)
{
  NyxGtkPreviousItemButton *self = NYX_GTK_PREVIOUS_ITEM_BUTTON_CAST (widget);

  g_clear_pointer (&self->n_items_binding, g_binding_unbind);
  g_clear_pointer (&self->current_index_binding, g_binding_unbind);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
nyx_gtk_previous_item_button_class_init (NyxGtkPreviousItemButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtkpreviousitembutton", 0,
      "Nyx GTK Previous Item Button");

  widget_class->map = nyx_gtk_previous_item_button_map;
  widget_class->unmap = nyx_gtk_previous_item_button_unmap;
}
