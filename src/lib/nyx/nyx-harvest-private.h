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
#include <gst/tag/tag.h>

#include "nyx-harvest.h"
#include "nyx-enhancer-proxy.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
NyxHarvest * nyx_harvest_new (void);

G_GNUC_INTERNAL
void nyx_harvest_set_enhancer_in_caps (NyxHarvest *harvest, NyxEnhancerProxy *proxy);

G_GNUC_INTERNAL
gboolean nyx_harvest_unpack (NyxHarvest *harvest, GstBuffer **buffer, gsize *buf_size, GstCaps **caps, GstTagList **tags, GstToc **toc, GstStructure **headers);

G_GNUC_INTERNAL
gboolean nyx_harvest_fill_from_cache (NyxHarvest *harvest, NyxEnhancerProxy *proxy, const GstStructure *config, GUri *uri);

G_GNUC_INTERNAL
void nyx_harvest_export_to_cache (NyxHarvest *harvest, NyxEnhancerProxy *proxy, const GstStructure *config, GUri *uri);

G_END_DECLS
