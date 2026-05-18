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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_LIMITED_LAYOUT (nyx_gtk_limited_layout_get_type())
#define NYX_GTK_LIMITED_LAYOUT_CAST(obj) ((NyxGtkLimitedLayout *)(obj))

G_DECLARE_FINAL_TYPE (NyxGtkLimitedLayout, nyx_gtk_limited_layout, NYX_GTK, LIMITED_LAYOUT, GtkLayoutManager)

G_GNUC_INTERNAL
void nyx_gtk_limited_layout_set_max_width (NyxGtkLimitedLayout *layout, gint max_width);

G_GNUC_INTERNAL
gint nyx_gtk_limited_layout_get_max_width (NyxGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void nyx_gtk_limited_layout_set_max_height (NyxGtkLimitedLayout *layout, gint max_height);

G_GNUC_INTERNAL
gint nyx_gtk_limited_layout_get_max_height (NyxGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void nyx_gtk_limited_layout_set_adaptive_width (NyxGtkLimitedLayout *layout, gint width);

G_GNUC_INTERNAL
gint nyx_gtk_limited_layout_get_adaptive_width (NyxGtkLimitedLayout *layout);

G_GNUC_INTERNAL
void nyx_gtk_limited_layout_set_adaptive_height (NyxGtkLimitedLayout *layout, gint height);

G_GNUC_INTERNAL
gint nyx_gtk_limited_layout_get_adaptive_height (NyxGtkLimitedLayout *layout);

G_END_DECLS
