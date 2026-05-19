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
#include <nyx/nyx-threaded-object.h>
#include <nyx/nyx-queue.h>
#include <nyx/nyx-stream-list.h>
#include <nyx/nyx-enhancer-proxy-list.h>
#include <nyx/nyx-feature.h>
#include <nyx/nyx-enums.h>

G_BEGIN_DECLS

#define NYX_TYPE_PLAYER (nyx_player_get_type())
#define NYX_PLAYER_CAST(obj) ((NyxPlayer *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxPlayer, nyx_player, NYX, PLAYER, NyxThreadedObject)

NYX_API
NyxPlayer * nyx_player_new (void);

NYX_API
NyxQueue * nyx_player_get_queue (NyxPlayer *player);

NYX_API
NyxStreamList * nyx_player_get_video_streams (NyxPlayer *player);

NYX_API
NyxStreamList * nyx_player_get_audio_streams (NyxPlayer *player);

NYX_API
NyxStreamList * nyx_player_get_subtitle_streams (NyxPlayer *player);

NYX_API
NyxEnhancerProxyList * nyx_player_get_enhancer_proxies (NyxPlayer *player);

NYX_API
void nyx_player_set_autoplay (NyxPlayer *player, gboolean enabled);

NYX_API
gboolean nyx_player_get_autoplay (NyxPlayer *player);

NYX_API
gdouble nyx_player_get_position (NyxPlayer *player);

NYX_API
NyxPlayerState nyx_player_get_state (NyxPlayer *player);

NYX_API
void nyx_player_set_mute (NyxPlayer *player, gboolean mute);

NYX_API
gboolean nyx_player_get_mute (NyxPlayer *player);

NYX_API
void nyx_player_set_volume (NyxPlayer *player, gdouble volume);

NYX_API
gdouble nyx_player_get_volume (NyxPlayer *player);

NYX_API
void nyx_player_set_speed (NyxPlayer *player, gdouble speed);

NYX_API
gdouble nyx_player_get_speed (NyxPlayer *player);

NYX_API
void nyx_player_set_video_sink (NyxPlayer *player, GstElement *element);

NYX_API
GstElement * nyx_player_get_video_sink (NyxPlayer *player);

NYX_API
void nyx_player_set_audio_sink (NyxPlayer *player, GstElement *element);

NYX_API
GstElement * nyx_player_get_audio_sink (NyxPlayer *player);

NYX_API
void nyx_player_set_video_filter (NyxPlayer *player, GstElement *element);

NYX_API
GstElement * nyx_player_get_video_filter (NyxPlayer *player);

NYX_API
void nyx_player_set_audio_filter (NyxPlayer *player, GstElement *element);

NYX_API
GstElement * nyx_player_get_audio_filter (NyxPlayer *player);

NYX_API
GstElement * nyx_player_get_current_video_decoder (NyxPlayer *player);

NYX_API
GstElement * nyx_player_get_current_audio_decoder (NyxPlayer *player);

NYX_API
void nyx_player_set_video_enabled (NyxPlayer *player, gboolean enabled);

NYX_API
gboolean nyx_player_get_video_enabled (NyxPlayer *player);

NYX_API
void nyx_player_set_audio_enabled (NyxPlayer *player, gboolean enabled);

NYX_API
gboolean nyx_player_get_audio_enabled (NyxPlayer *player);

NYX_API
void nyx_player_set_subtitles_enabled (NyxPlayer *player, gboolean enabled);

NYX_API
gboolean nyx_player_get_subtitles_enabled (NyxPlayer *player);

NYX_API
void nyx_player_set_download_dir (NyxPlayer *player, const gchar *path);

NYX_API
gchar * nyx_player_get_download_dir (NyxPlayer *player);

NYX_API
void nyx_player_set_download_enabled (NyxPlayer *player, gboolean enabled);

NYX_API
gboolean nyx_player_get_download_enabled (NyxPlayer *player);

NYX_API
void nyx_player_set_adaptive_start_bitrate (NyxPlayer *player, guint bitrate);

NYX_API
guint nyx_player_get_adaptive_start_bitrate (NyxPlayer *player);

NYX_API
void nyx_player_set_adaptive_min_bitrate (NyxPlayer *player, guint bitrate);

NYX_API
guint nyx_player_get_adaptive_min_bitrate (NyxPlayer *player);

NYX_API
void nyx_player_set_adaptive_max_bitrate (NyxPlayer *player, guint bitrate);

NYX_API
guint nyx_player_get_adaptive_max_bitrate (NyxPlayer *player);

NYX_API
guint nyx_player_get_adaptive_bandwidth (NyxPlayer *player);

NYX_API
void nyx_player_set_audio_offset (NyxPlayer *player, gdouble offset);

NYX_API
gdouble nyx_player_get_audio_offset (NyxPlayer *player);

NYX_API
void nyx_player_set_subtitle_offset (NyxPlayer *player, gdouble offset);

NYX_API
gdouble nyx_player_get_subtitle_offset (NyxPlayer *player);

NYX_API
void nyx_player_set_subtitle_font_desc (NyxPlayer *player, const gchar *font_desc);

NYX_API
gchar * nyx_player_get_subtitle_font_desc (NyxPlayer *player);

NYX_API
void nyx_player_play (NyxPlayer *player);

NYX_API
void nyx_player_pause (NyxPlayer *player);

NYX_API
void nyx_player_stop (NyxPlayer *player);

NYX_API
void nyx_player_seek (NyxPlayer *player, gdouble position);

NYX_API
void nyx_player_seek_custom (NyxPlayer *player, gdouble position, NyxPlayerSeekMethod method);

NYX_API
void nyx_player_advance_frame (NyxPlayer *player);

NYX_API
void nyx_player_add_feature (NyxPlayer *player, NyxFeature *feature);

NYX_API
gchar * nyx_player_make_pipeline_graph (NyxPlayer *player, GstDebugGraphDetails details);

NYX_API
void nyx_player_post_message (NyxPlayer *player, GstMessage *msg, NyxPlayerMessageDestination destination);

G_END_DECLS
