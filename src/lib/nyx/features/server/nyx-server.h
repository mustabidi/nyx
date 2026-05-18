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

#define NYX_TYPE_SERVER (nyx_server_get_type())
#define NYX_SERVER_CAST(obj) ((NyxServer *)(obj))

NYX_DEPRECATED
G_DECLARE_FINAL_TYPE (NyxServer, nyx_server, NYX, SERVER, NyxFeature)

NYX_DEPRECATED
NyxServer * nyx_server_new (void);

NYX_DEPRECATED
void nyx_server_set_enabled (NyxServer *server, gboolean enabled);

NYX_DEPRECATED
gboolean nyx_server_get_enabled (NyxServer *server);

NYX_DEPRECATED
gboolean nyx_server_get_running (NyxServer *server);

NYX_DEPRECATED
void nyx_server_set_port (NyxServer *server, guint port);

NYX_DEPRECATED
guint nyx_server_get_port (NyxServer *server);

NYX_DEPRECATED
guint nyx_server_get_current_port (NyxServer *server);

NYX_DEPRECATED
void nyx_server_set_queue_controllable (NyxServer *server, gboolean controllable);

NYX_DEPRECATED
gboolean nyx_server_get_queue_controllable (NyxServer *server);

G_END_DECLS
