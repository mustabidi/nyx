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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>

G_BEGIN_DECLS

#define NYX_TYPE_ENHANCER_PROXY (nyx_enhancer_proxy_get_type())
#define NYX_ENHANCER_PROXY_CAST(obj) ((NyxEnhancerProxy *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxEnhancerProxy, nyx_enhancer_proxy, NYX, ENHANCER_PROXY, GstObject)

NYX_API
const gchar * nyx_enhancer_proxy_get_friendly_name (NyxEnhancerProxy *proxy);

NYX_API
const gchar * nyx_enhancer_proxy_get_module_name (NyxEnhancerProxy *proxy);

NYX_API
const gchar * nyx_enhancer_proxy_get_module_dir (NyxEnhancerProxy *proxy);

NYX_API
const gchar * nyx_enhancer_proxy_get_description (NyxEnhancerProxy *proxy);

NYX_API
const gchar * nyx_enhancer_proxy_get_version (NyxEnhancerProxy *proxy);

NYX_API
const gchar * nyx_enhancer_proxy_get_extra_data (NyxEnhancerProxy *proxy, const gchar *key);

NYX_API
gboolean nyx_enhancer_proxy_extra_data_lists_value (NyxEnhancerProxy *proxy, const gchar *key, const gchar *value);

NYX_API
GType * nyx_enhancer_proxy_get_target_interfaces (NyxEnhancerProxy *proxy, guint *n_interfaces);

NYX_API
gboolean nyx_enhancer_proxy_target_has_interface (NyxEnhancerProxy *proxy, GType iface_type);

NYX_API
GParamSpec ** nyx_enhancer_proxy_get_target_properties (NyxEnhancerProxy *proxy, guint *n_properties);

NYX_API
GSettings * nyx_enhancer_proxy_get_settings (NyxEnhancerProxy *proxy);

NYX_API
void nyx_enhancer_proxy_set_locally (NyxEnhancerProxy *proxy, const gchar *first_property_name, ...) G_GNUC_NULL_TERMINATED;

NYX_API
void nyx_enhancer_proxy_set_locally_with_table (NyxEnhancerProxy *proxy, GHashTable *table);

NYX_API
void nyx_enhancer_proxy_set_target_creation_allowed (NyxEnhancerProxy *proxy, gboolean allowed);

NYX_API
gboolean nyx_enhancer_proxy_get_target_creation_allowed (NyxEnhancerProxy *proxy);

G_END_DECLS
