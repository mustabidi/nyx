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

#include "nyx-gtk-container.h"

G_BEGIN_DECLS

#define NYX_GTK_TYPE_BUFFERING_ANIMATION (nyx_gtk_buffering_animation_get_type())
#define NYX_GTK_BUFFERING_ANIMATION_CAST(obj) ((NyxGtkBufferingAnimation *)(obj))

G_DECLARE_FINAL_TYPE (NyxGtkBufferingAnimation, nyx_gtk_buffering_animation, NYX_GTK, BUFFERING_ANIMATION, NyxGtkContainer)

G_GNUC_INTERNAL
void nyx_gtk_buffering_animation_start (NyxGtkBufferingAnimation *animation);

G_GNUC_INTERNAL
void nyx_gtk_buffering_animation_stop (NyxGtkBufferingAnimation *animation);

G_END_DECLS
