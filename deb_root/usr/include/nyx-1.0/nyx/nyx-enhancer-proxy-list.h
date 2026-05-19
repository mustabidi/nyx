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
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-enhancer-proxy.h>

G_BEGIN_DECLS

#define NYX_TYPE_ENHANCER_PROXY_LIST (nyx_enhancer_proxy_list_get_type())
#define NYX_ENHANCER_PROXY_LIST_CAST(obj) ((NyxEnhancerProxyList *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxEnhancerProxyList, nyx_enhancer_proxy_list, NYX, ENHANCER_PROXY_LIST, GstObject)

NYX_API
NyxEnhancerProxy * nyx_enhancer_proxy_list_get_proxy (NyxEnhancerProxyList *list, guint index);

NYX_API
NyxEnhancerProxy * nyx_enhancer_proxy_list_peek_proxy (NyxEnhancerProxyList *list, guint index);

NYX_API
NyxEnhancerProxy * nyx_enhancer_proxy_list_get_proxy_by_module (NyxEnhancerProxyList *list, const gchar *module_name);

NYX_API
guint nyx_enhancer_proxy_list_get_n_proxies (NyxEnhancerProxyList *list);

G_END_DECLS
