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

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-enhancer-proxy-list.h>

G_BEGIN_DECLS

NYX_API
void nyx_init (int *argc, char **argv[]);

NYX_API
gboolean nyx_init_check (int *argc, char **argv[]);

NYX_DEPRECATED
gboolean nyx_enhancer_check (GType iface_type, const gchar *scheme, const gchar *host, const gchar **name);

NYX_API
NyxEnhancerProxyList * nyx_get_global_enhancer_proxies (void);

G_END_DECLS
