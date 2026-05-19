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

#include <nyx-gtk/nyx-gtk-av.h>
#include <nyx-gtk/nyx-gtk-visibility.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_VIDEO (nyx_gtk_video_get_type())
#define NYX_GTK_VIDEO_CAST(obj) ((NyxGtkVideo *)(obj))

NYX_GTK_API
G_DECLARE_FINAL_TYPE (NyxGtkVideo, nyx_gtk_video, NYX_GTK, VIDEO, NyxGtkAv)

NYX_GTK_API
GtkWidget * nyx_gtk_video_new (void);

NYX_GTK_API
void nyx_gtk_video_add_overlay (NyxGtkVideo *video, GtkWidget *widget);

NYX_GTK_API
void nyx_gtk_video_add_fading_overlay (NyxGtkVideo *video, GtkWidget *widget);

NYX_GTK_DEPRECATED_FOR(nyx_gtk_av_get_player)
NyxPlayer * nyx_gtk_video_get_player (NyxGtkVideo *video);

NYX_GTK_API
void nyx_gtk_video_set_fade_delay (NyxGtkVideo *video, guint delay);

NYX_GTK_API
guint nyx_gtk_video_get_fade_delay (NyxGtkVideo *video);

NYX_GTK_API
void nyx_gtk_video_set_touch_fade_delay (NyxGtkVideo *video, guint delay);

NYX_GTK_API
guint nyx_gtk_video_get_touch_fade_delay (NyxGtkVideo *video);

NYX_GTK_DEPRECATED_FOR(nyx_gtk_av_set_auto_inhibit)
void nyx_gtk_video_set_auto_inhibit (NyxGtkVideo *video, gboolean inhibit);

NYX_GTK_DEPRECATED_FOR(nyx_gtk_av_get_auto_inhibit)
gboolean nyx_gtk_video_get_auto_inhibit (NyxGtkVideo *video);

NYX_GTK_DEPRECATED_FOR(nyx_gtk_av_get_inhibited)
gboolean nyx_gtk_video_get_inhibited (NyxGtkVideo *video);

G_END_DECLS
