/* Nyx Playback Library
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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-marker.h>

G_BEGIN_DECLS

#define NYX_TYPE_TIMELINE (nyx_timeline_get_type())
#define NYX_TIMELINE_CAST(obj) ((NyxTimeline *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxTimeline, nyx_timeline, NYX, TIMELINE, GstObject)

NYX_API
void nyx_timeline_insert_marker (NyxTimeline *timeline, NyxMarker *marker);

NYX_API
void nyx_timeline_remove_marker (NyxTimeline *timeline, NyxMarker *marker);

NYX_API
NyxMarker * nyx_timeline_get_marker (NyxTimeline *timeline, guint index);

NYX_API
guint nyx_timeline_get_n_markers (NyxTimeline *timeline);

G_END_DECLS
