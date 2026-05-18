/*
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

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-feature.h>

G_BEGIN_DECLS

#define NYX_TYPE_DISCOVERER (nyx_discoverer_get_type())
#define NYX_DISCOVERER_CAST(obj) ((NyxDiscoverer *)(obj))

NYX_DEPRECATED
G_DECLARE_FINAL_TYPE (NyxDiscoverer, nyx_discoverer, NYX, DISCOVERER, NyxFeature)

NYX_DEPRECATED
NyxDiscoverer * nyx_discoverer_new (void);

NYX_DEPRECATED
void nyx_discoverer_set_discovery_mode (NyxDiscoverer *discoverer, NyxDiscovererDiscoveryMode mode);

NYX_DEPRECATED
NyxDiscovererDiscoveryMode nyx_discoverer_get_discovery_mode (NyxDiscoverer *discoverer);

G_END_DECLS
