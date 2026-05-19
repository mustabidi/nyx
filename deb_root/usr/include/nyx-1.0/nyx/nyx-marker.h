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
#include <nyx/nyx-enums.h>

G_BEGIN_DECLS

#define NYX_TYPE_MARKER (nyx_marker_get_type())
#define NYX_MARKER_CAST(obj) ((NyxMarker *)(obj))

/* NOTE: #NyxMarker are immutable objects that cannot be derived,
 * otherwise #NyxFeaturesManager would not be able to announce media
 * item changed caused by changes within them */
NYX_API
G_DECLARE_FINAL_TYPE (NyxMarker, nyx_marker, NYX, MARKER, GstObject)

/**
 * NYX_MARKER_NO_END:
 *
 * The value used to indicate that marker does not have an ending time specified
 */
#define NYX_MARKER_NO_END ((gdouble) -1) // Needs a cast from int, otherwise GIR is generated incorrectly

NYX_API
NyxMarker * nyx_marker_new (NyxMarkerType marker_type, const gchar *title, gdouble start, gdouble end);

NYX_API
NyxMarkerType nyx_marker_get_marker_type (NyxMarker *marker);

NYX_API
const gchar * nyx_marker_get_title (NyxMarker *marker);

NYX_API
gdouble nyx_marker_get_start (NyxMarker *marker);

NYX_API
gdouble nyx_marker_get_end (NyxMarker *marker);

G_END_DECLS
