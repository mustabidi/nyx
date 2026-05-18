/* Nyx Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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
#include <gst/gst.h>

#include "nyx-uri-base-demux-private.h"

G_BEGIN_DECLS

#define NYX_TYPE_HARVEST_URI_DEMUX (nyx_harvest_uri_demux_get_type())
#define NYX_HARVEST_URI_DEMUX_CAST(obj) ((NyxHarvestUriDemux *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (NyxHarvestUriDemux, nyx_harvest_uri_demux, NYX, HARVEST_URI_DEMUX, NyxUriBaseDemux)

GST_ELEMENT_REGISTER_DECLARE (nyxharvesturidemux)

G_END_DECLS
