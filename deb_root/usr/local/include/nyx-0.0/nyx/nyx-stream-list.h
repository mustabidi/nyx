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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-stream.h>

G_BEGIN_DECLS

#define NYX_TYPE_STREAM_LIST (nyx_stream_list_get_type())
#define NYX_STREAM_LIST_CAST(obj) ((NyxStreamList *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxStreamList, nyx_stream_list, NYX, STREAM_LIST, GstObject)

/**
 * NYX_STREAM_LIST_INVALID_POSITION:
 *
 * The value used to refer to an invalid position in a #NyxStreamList
 */
#define NYX_STREAM_LIST_INVALID_POSITION ((guint) 0xffffffff)

NYX_API
gboolean nyx_stream_list_select_stream (NyxStreamList *list, NyxStream *stream);

NYX_API
gboolean nyx_stream_list_select_index (NyxStreamList *list, guint index);

NYX_API
NyxStream * nyx_stream_list_get_stream (NyxStreamList *list, guint index);

NYX_API
NyxStream * nyx_stream_list_get_current_stream (NyxStreamList *list);

NYX_API
guint nyx_stream_list_get_current_index (NyxStreamList *list);

NYX_API
guint nyx_stream_list_get_n_streams (NyxStreamList *list);

G_END_DECLS
