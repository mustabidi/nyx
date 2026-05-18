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

#include <glib.h>

#include "nyx-enums.h"
#include "nyx-marker.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
NyxMarker * nyx_marker_new_internal (NyxMarkerType marker_type, const gchar *name, gdouble start, gdouble end);

G_GNUC_INTERNAL
gboolean nyx_marker_is_internal (NyxMarker *marker);

G_END_DECLS
