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

#define NYX_GTK_TYPE_TOGGLE_PLAY_BUTTON (nyx_gtk_toggle_play_button_get_type())
#define NYX_GTK_TOGGLE_PLAY_BUTTON_CAST(obj) ((NyxGtkTogglePlayButton *)(obj))

NYX_GTK_API
G_DECLARE_FINAL_TYPE (NyxGtkTogglePlayButton, nyx_gtk_toggle_play_button, NYX_GTK, TOGGLE_PLAY_BUTTON, GtkButton)

NYX_GTK_API
GtkWidget * nyx_gtk_toggle_play_button_new (void);

G_END_DECLS
