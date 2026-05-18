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
#include <nyx-gtk/nyx-gtk-lead-container.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_TITLE_HEADER (nyx_gtk_title_header_get_type())
#define NYX_GTK_TITLE_HEADER_CAST(obj) ((NyxGtkTitleHeader *)(obj))

NYX_GTK_API
G_DECLARE_FINAL_TYPE (NyxGtkTitleHeader, nyx_gtk_title_header, NYX_GTK, TITLE_HEADER, NyxGtkLeadContainer)

NYX_GTK_API
GtkWidget * nyx_gtk_title_header_new (void);

NYX_GTK_API
const gchar * nyx_gtk_title_header_get_current_title (NyxGtkTitleHeader *header);

NYX_GTK_API
void nyx_gtk_title_header_set_fallback_to_uri (NyxGtkTitleHeader *header, gboolean enabled);

NYX_GTK_API
gboolean nyx_gtk_title_header_get_fallback_to_uri (NyxGtkTitleHeader *header);

G_END_DECLS
