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
#include <nyx/nyx.h>

#include <nyx-gtk/nyx-gtk-visibility.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_TITLE_LABEL (nyx_gtk_title_label_get_type())
#define NYX_GTK_TITLE_LABEL_CAST(obj) ((NyxGtkTitleLabel *)(obj))

NYX_GTK_API
G_DECLARE_FINAL_TYPE (NyxGtkTitleLabel, nyx_gtk_title_label, NYX_GTK, TITLE_LABEL, GtkWidget)

NYX_GTK_API
GtkWidget * nyx_gtk_title_label_new (void);

NYX_GTK_API
void nyx_gtk_title_label_set_media_item (NyxGtkTitleLabel *label, NyxMediaItem *item);

NYX_GTK_API
NyxMediaItem * nyx_gtk_title_label_get_media_item (NyxGtkTitleLabel *label);

NYX_GTK_API
const gchar * nyx_gtk_title_label_get_current_title (NyxGtkTitleLabel *label);

NYX_GTK_API
void nyx_gtk_title_label_set_fallback_to_uri (NyxGtkTitleLabel *label, gboolean enabled);

NYX_GTK_API
gboolean nyx_gtk_title_label_get_fallback_to_uri (NyxGtkTitleLabel *label);

G_END_DECLS
