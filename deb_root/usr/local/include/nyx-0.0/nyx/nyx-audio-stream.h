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

#define NYX_TYPE_AUDIO_STREAM (nyx_audio_stream_get_type())
#define NYX_AUDIO_STREAM_CAST(obj) ((NyxAudioStream *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxAudioStream, nyx_audio_stream, NYX, AUDIO_STREAM, NyxStream)

NYX_API
gchar * nyx_audio_stream_get_codec (NyxAudioStream *stream);

NYX_API
guint nyx_audio_stream_get_bitrate (NyxAudioStream *stream);

NYX_API
gchar * nyx_audio_stream_get_sample_format (NyxAudioStream *stream);

NYX_API
gint nyx_audio_stream_get_sample_rate (NyxAudioStream *stream);

NYX_API
gint nyx_audio_stream_get_channels (NyxAudioStream *stream);

NYX_API
gchar * nyx_audio_stream_get_lang_code (NyxAudioStream *stream);

NYX_API
gchar * nyx_audio_stream_get_lang_name (NyxAudioStream *stream);

G_END_DECLS
