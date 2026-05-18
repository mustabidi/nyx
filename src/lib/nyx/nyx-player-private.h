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

#include <gst/tag/tag.h>

#include "nyx-player.h"
#include "nyx-queue.h"
#include "nyx-enums.h"

#include "nyx-app-bus-private.h"
#include "nyx-features-manager-private.h"
#include "nyx-reactables-manager-private.h"

G_BEGIN_DECLS

#define nyx_player_set_have_features(player,have) (g_atomic_int_set (&player->have_features, (gint) have))
#define nyx_player_get_have_features(player) (g_atomic_int_get (&player->have_features) == 1)
#define nyx_player_get_features_manager(player) (nyx_player_get_have_features(player) ? player->features_manager : NULL)

struct _NyxPlayer
{
  NyxThreadedObject parent;

  NyxQueue *queue;

  NyxStreamList *video_streams;
  NyxStreamList *audio_streams;
  NyxStreamList *subtitle_streams;

  NyxFeaturesManager *features_manager;
  gint have_features; // atomic integer

  NyxReactablesManager *reactables_manager;

  NyxEnhancerProxyList *enhancer_proxies;

  /* This is different from queue current item as it is used/changed only
   * on player thread, so we can always update correct item without lock */
  NyxMediaItem *played_item;

  /* Will eventually become our "played_item", can be set from
   * different thread, thus needs a lock */
  NyxMediaItem *pending_item;

  /* Pending tags/toc that arrive before stream start.
   * To be applied to "played_item", thus no lock needed. */
  gboolean stream_tags_allowed;
  GstTagList *pending_tags;
  GstToc *pending_toc;

  GstElement *playbin;

  GstBus *bus;
  NyxAppBus *app_bus;

  GSource *tick_source;
  GstQuery *position_query;

  /* Must only be used from player thread */
  GstState current_state; // reported from playbin
  GstState target_state;  // state requested by user
  gboolean is_buffering;
  gdouble pending_position; // store seek before playback
  gdouble requested_speed, pending_speed; // store speed for consecutive rate changes

  /* Stream collection */
  GstStreamCollection *collection;
  gulong stream_notify_id;

  /* Extra params */
  gboolean use_playbin3; // when using playbin3
  gboolean had_error; // so we do not do stuff after error
  gboolean seeking; // during seek operation
  gboolean stepping; // during frame step operation
  gboolean speed_changing; // during rate change operation
  gboolean pending_eos; // when pausing due to EOS
  gboolean pending_flush; // after another stream selection
  gint eos; // atomic integer

  /* Set adaptive props immediately */
  GstElement *adaptive_demuxer;

  /* Playbin2 compat */
  gint n_video, n_audio, n_text;

  /* Props */
  gboolean autoplay;
  gboolean mute;
  gdouble volume;
  gdouble speed;
  gdouble position;
  NyxPlayerState state;
  GstElement *video_decoder;
  GstElement *audio_decoder;
  gboolean video_enabled;
  gboolean audio_enabled;
  gboolean subtitles_enabled;
  gchar *download_dir;
  gboolean download_enabled;
  guint start_bitrate;
  guint min_bitrate;
  guint max_bitrate;
  guint bandwidth;
  gdouble audio_offset;
  gdouble subtitle_offset;
};

NyxPlayer * nyx_player_get_from_ancestor (GstObject *object);

gboolean nyx_player_refresh_position (NyxPlayer *player);

void nyx_player_add_tick_source (NyxPlayer *player);

void nyx_player_remove_tick_source (NyxPlayer *player);

void nyx_player_handle_playbin_state_changed (NyxPlayer *player);

void nyx_player_handle_playbin_volume_changed (NyxPlayer *player, const GValue *value);

void nyx_player_handle_playbin_mute_changed (NyxPlayer *player, const GValue *value);

void nyx_player_handle_playbin_flags_changed (NyxPlayer *player, const GValue *value);

void nyx_player_handle_playbin_av_offset_changed (NyxPlayer *player, const GValue *value);

void nyx_player_handle_playbin_text_offset_changed (NyxPlayer *player, const GValue *value);

void nyx_player_handle_playbin_common_prop_changed (NyxPlayer *player, const gchar *prop_name);

void nyx_player_handle_playbin_rate_changed (NyxPlayer *player, gdouble speed);

void nyx_player_set_pending_item (NyxPlayer *player, NyxMediaItem *pending_item, NyxQueueItemChangeMode mode);

void nyx_player_take_stream_collection (NyxPlayer *player, GstStreamCollection *collection);

void nyx_player_refresh_streams (NyxPlayer *player);

gboolean nyx_player_find_active_decoder_with_stream_id (NyxPlayer *player, GstElementFactoryListType type, const gchar *stream_id);

void nyx_player_playbin_update_current_decoders (NyxPlayer *player);

void nyx_player_reset (NyxPlayer *player, gboolean pending_dispose);

G_END_DECLS
