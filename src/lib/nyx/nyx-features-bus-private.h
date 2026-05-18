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

#include <gst/gst.h>
#include <glib-object.h>

#include "nyx-features-manager-private.h"
#include "nyx-enums-private.h"

G_BEGIN_DECLS

#define NYX_TYPE_FEATURES_BUS (nyx_features_bus_get_type())
#define NYX_FEATURES_BUS_CAST(obj) ((NyxFeaturesBus *)(obj))

/**
 * NyxFeaturesBus:
 */
G_DECLARE_FINAL_TYPE (NyxFeaturesBus, nyx_features_bus, NYX, FEATURES_BUS, GstBus)

void nyx_features_bus_initialize (void);

NyxFeaturesBus * nyx_features_bus_new (void);

void nyx_features_bus_post_event (NyxFeaturesBus *features_bus, NyxFeaturesManager *src, NyxFeaturesManagerEvent event, GValue *value, GValue *extra_value);

G_END_DECLS
