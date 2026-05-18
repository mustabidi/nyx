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
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include "../nyx-threaded-object.h"
#include "../nyx-harvest.h"

G_BEGIN_DECLS

#define NYX_TYPE_ENHANCER_DIRECTOR (nyx_enhancer_director_get_type())
#define NYX_ENHANCER_DIRECTOR_CAST(obj) ((NyxEnhancerDirector *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (NyxEnhancerDirector, nyx_enhancer_director, NYX, ENHANCER_DIRECTOR, NyxThreadedObject)

G_GNUC_INTERNAL
NyxEnhancerDirector * nyx_enhancer_director_new (void);

G_GNUC_INTERNAL
NyxHarvest * nyx_enhancer_director_extract (NyxEnhancerDirector *director, GList *filtered_proxies, GUri *uri, GCancellable *cancellable, GError **error);

G_GNUC_INTERNAL
GListStore * nyx_enhancer_director_parse (NyxEnhancerDirector *director, GList *filtered_proxies, GUri *uri, GstBuffer *buffer, GCancellable *cancellable, GError **error);

G_END_DECLS
