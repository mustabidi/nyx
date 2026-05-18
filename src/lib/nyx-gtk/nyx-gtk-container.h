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

#pragma once

#if !defined(__NYX_GTK_INSIDE__) && !defined(NYX_GTK_COMPILATION)
#error "Only <nyx-gtk/nyx-gtk.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <nyx-gtk/nyx-gtk-visibility.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_CONTAINER (nyx_gtk_container_get_type())
#define NYX_GTK_CONTAINER_CAST(obj) ((NyxGtkContainer *)(obj))

NYX_GTK_API
G_DECLARE_DERIVABLE_TYPE (NyxGtkContainer, nyx_gtk_container, NYX_GTK, CONTAINER, GtkWidget)

struct _NyxGtkContainerClass
{
  GtkWidgetClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

NYX_GTK_API
GtkWidget * nyx_gtk_container_new (void);

NYX_GTK_API
void nyx_gtk_container_set_child (NyxGtkContainer *container, GtkWidget *child);

NYX_GTK_API
GtkWidget * nyx_gtk_container_get_child (NyxGtkContainer *container);

NYX_GTK_API
void nyx_gtk_container_set_width_target (NyxGtkContainer *container, gint width);

NYX_GTK_API
gint nyx_gtk_container_get_width_target (NyxGtkContainer *container);

NYX_GTK_API
void nyx_gtk_container_set_height_target (NyxGtkContainer *container, gint height);

NYX_GTK_API
gint nyx_gtk_container_get_height_target (NyxGtkContainer *container);

NYX_GTK_API
void nyx_gtk_container_set_adaptive_width (NyxGtkContainer *container, gint width);

NYX_GTK_API
gint nyx_gtk_container_get_adaptive_width (NyxGtkContainer *container);

NYX_GTK_API
void nyx_gtk_container_set_adaptive_height (NyxGtkContainer *container, gint height);

NYX_GTK_API
gint nyx_gtk_container_get_adaptive_height (NyxGtkContainer *container);

G_END_DECLS
