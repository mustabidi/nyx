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
#include <nyx-gtk/nyx-gtk-container.h>
#include <nyx-gtk/nyx-gtk-extra-menu-button.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_SIMPLE_CONTROLS (nyx_gtk_simple_controls_get_type())
#define NYX_GTK_SIMPLE_CONTROLS_CAST(obj) ((NyxGtkSimpleControls *)(obj))

NYX_GTK_API
G_DECLARE_FINAL_TYPE (NyxGtkSimpleControls, nyx_gtk_simple_controls, NYX_GTK, SIMPLE_CONTROLS, NyxGtkContainer)

NYX_GTK_API
GtkWidget * nyx_gtk_simple_controls_new (void);

NYX_GTK_API
void nyx_gtk_simple_controls_set_fullscreenable (NyxGtkSimpleControls *controls, gboolean fullscreenable);

NYX_GTK_API
gboolean nyx_gtk_simple_controls_get_fullscreenable (NyxGtkSimpleControls *controls);

NYX_GTK_API
void nyx_gtk_simple_controls_set_seek_method (NyxGtkSimpleControls *controls, NyxPlayerSeekMethod method);

NYX_GTK_API
NyxPlayerSeekMethod nyx_gtk_simple_controls_get_seek_method (NyxGtkSimpleControls *controls);

NYX_GTK_API
NyxGtkExtraMenuButton * nyx_gtk_simple_controls_get_extra_menu_button (NyxGtkSimpleControls *controls);

G_END_DECLS
