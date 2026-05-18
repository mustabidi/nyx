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
#include <nyx-gtk/nyx-gtk-enum-types.h>

G_BEGIN_DECLS

/**
 * NyxGtkVideoActionMask:
 * @NYX_GTK_VIDEO_ACTION_NONE: No action
 * @NYX_GTK_VIDEO_ACTION_REVEAL_OVERLAYS: Reveal fading overlays
 * @NYX_GTK_VIDEO_ACTION_TOGGLE_PLAY: Toggle playback (triggered by single click/tap)
 * @NYX_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN: Toggle fullscreen (triggered by double click/tap)
 * @NYX_GTK_VIDEO_ACTION_SEEK_REQUEST: Seek request (triggered by double tap on screen side)
 * @NYX_GTK_VIDEO_ACTION_ANY: All of the above
 */
typedef enum
{
  NYX_GTK_VIDEO_ACTION_NONE = 0,
  NYX_GTK_VIDEO_ACTION_REVEAL_OVERLAYS = 1 << 0,
  NYX_GTK_VIDEO_ACTION_TOGGLE_PLAY = 1 << 1,
  NYX_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN = 1 << 2,
  NYX_GTK_VIDEO_ACTION_SEEK_REQUEST = 1 << 3,
  NYX_GTK_VIDEO_ACTION_ANY = 0x3FFFFFF
} NyxGtkVideoActionMask;

G_END_DECLS
