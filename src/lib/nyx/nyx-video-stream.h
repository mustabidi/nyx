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

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-stream.h>

G_BEGIN_DECLS

#define NYX_TYPE_VIDEO_STREAM (nyx_video_stream_get_type())
#define NYX_VIDEO_STREAM_CAST(obj) ((NyxVideoStream *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxVideoStream, nyx_video_stream, NYX, VIDEO_STREAM, NyxStream)

NYX_API
gchar * nyx_video_stream_get_codec (NyxVideoStream *stream);

NYX_API
gint nyx_video_stream_get_width (NyxVideoStream *stream);

NYX_API
gint nyx_video_stream_get_height (NyxVideoStream *stream);

NYX_API
gdouble nyx_video_stream_get_fps (NyxVideoStream *stream);

NYX_API
guint nyx_video_stream_get_bitrate (NyxVideoStream *stream);

NYX_API
gchar * nyx_video_stream_get_pixel_format (NyxVideoStream *stream);

G_END_DECLS
