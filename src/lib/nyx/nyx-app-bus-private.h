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
#include <gst/gst.h>

G_BEGIN_DECLS

#define NYX_TYPE_APP_BUS (nyx_app_bus_get_type())
#define NYX_APP_BUS_CAST(obj) ((NyxAppBus *)(obj))

/**
 * NyxAppBus:
 */
G_DECLARE_FINAL_TYPE (NyxAppBus, nyx_app_bus, NYX, APP_BUS, GstBus)

void nyx_app_bus_initialize (void);

NyxAppBus * nyx_app_bus_new (void);

void nyx_app_bus_forward_message (NyxAppBus *app_bus, GstMessage *msg);

void nyx_app_bus_post_prop_notify (NyxAppBus *app_bus, GstObject *src, GParamSpec *pspec);

void nyx_app_bus_post_refresh_streams (NyxAppBus *app_bus, GstObject *src);

void nyx_app_bus_post_refresh_timeline (NyxAppBus *app_bus, GstObject *src);

void nyx_app_bus_post_insert_playlist (NyxAppBus *app_bus, GstObject *src, GstObject *playlist_item, GObject *playlist);

void nyx_app_bus_post_simple_signal (NyxAppBus *app_bus, GstObject *src, guint signal_id);

void nyx_app_bus_post_object_desc_signal (NyxAppBus *app_bus, GstObject *src, guint signal_id, GstObject *object, const gchar *desc);

void nyx_app_bus_post_desc_with_details_signal (NyxAppBus *app_bus, GstObject *src, guint signal_id, const gchar *desc, const gchar *details);

void nyx_app_bus_post_message_signal (NyxAppBus *app_bus, GstObject *src, guint signal_id, GstMessage *msg);

void nyx_app_bus_post_error_signal (NyxAppBus *app_bus, GstObject *src, guint signal_id, GError *error, const gchar *debug_info);

G_END_DECLS
