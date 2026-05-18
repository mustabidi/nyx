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

#define NYX_GTK_TYPE_BUFFERING_PAINTABLE (nyx_gtk_buffering_paintable_get_type())
#define NYX_GTK_BUFFERING_PAINTABLE_CAST(obj) ((NyxGtkBufferingPaintable *)(obj))

G_DECLARE_FINAL_TYPE (NyxGtkBufferingPaintable, nyx_gtk_buffering_paintable, NYX_GTK, BUFFERING_PAINTABLE, GObject)

G_GNUC_INTERNAL
NyxGtkBufferingPaintable * nyx_gtk_buffering_paintable_new (void);

G_GNUC_INTERNAL
void nyx_gtk_buffering_paintable_step (NyxGtkBufferingPaintable *buffering_paintable);

G_GNUC_INTERNAL
void nyx_gtk_buffering_paintable_reset (NyxGtkBufferingPaintable *buffering_paintable);

G_END_DECLS
