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

#include "nyx-enhancer-proxy.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
NyxEnhancerProxy * nyx_enhancer_proxy_new_global_take (GObject *peas_info); // Using parent type for building without libpeas

G_GNUC_INTERNAL
NyxEnhancerProxy * nyx_enhancer_proxy_copy (NyxEnhancerProxy *src_proxy, const gchar *copy_name);

G_GNUC_INTERNAL
gboolean nyx_enhancer_proxy_fill_from_cache (NyxEnhancerProxy *proxy);

G_GNUC_INTERNAL
gboolean nyx_enhancer_proxy_fill_from_instance (NyxEnhancerProxy *proxy, GObject *enhancer);

G_GNUC_INTERNAL
void nyx_enhancer_proxy_export_to_cache (NyxEnhancerProxy *proxy);

G_GNUC_INTERNAL
GObject * nyx_enhancer_proxy_get_peas_info (NyxEnhancerProxy *proxy);

G_GNUC_INTERNAL
gboolean nyx_enhancer_proxy_has_locally_set (NyxEnhancerProxy *proxy, const gchar *property_name);

G_GNUC_INTERNAL
GstStructure * nyx_enhancer_proxy_make_current_config (NyxEnhancerProxy *proxy);

G_GNUC_INTERNAL
void nyx_enhancer_proxy_apply_config_to_enhancer (NyxEnhancerProxy *proxy, const GstStructure *config, GObject *enhancer);

G_GNUC_INTERNAL
void nyx_enhancer_proxy_await_job_start (NyxEnhancerProxy *proxy, guint job_id);

G_GNUC_INTERNAL
void nyx_enhancer_proxy_remove_job (NyxEnhancerProxy *proxy, guint job_id);

G_END_DECLS
