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

/**
 * NyxPlayer:
 *
 * The media player object used for playback.
 *
 * #NyxPlayer was written in an easy to use way, so no special GStreamer
 * experience is needed to get started with making various playback applications.
 *
 * Scheduling media for playback is done using a [class@Nyx.Queue] upon which
 * player operates.
 *
 * Player uses `GStreamer` internally and handles playback on a separate thread, while
 * serializing all events/commands between player and the thread it was created upon
 * (usually main app thread). This makes it very easy to integrate with UI toolkits
 * that operate on a single thread like (but not limited to) GTK.
 *
 * To listen for property changes, you can connect to property "notify" signal.
 */

#include <gst/audio/streamvolume.h>

#include "nyx-player.h"
#include "nyx-player-private.h"
#include "nyx-playbin-bus-private.h"
#include "nyx-app-bus-private.h"
#include "nyx-queue-private.h"
#include "nyx-media-item-private.h"
#include "nyx-stream-list-private.h"
#include "nyx-stream-private.h"
#include "nyx-video-stream-private.h"
#include "nyx-audio-stream-private.h"
#include "nyx-subtitle-stream-private.h"
#include "nyx-enhancer-proxy-list-private.h"
#include "nyx-reactable.h"
#include "nyx-enums-private.h"
#include "nyx-utils-private.h"
#include "../shared/nyx-shared-utils-private.h"

#define DEFAULT_AUTOPLAY FALSE
#define DEFAULT_MUTE FALSE
#define DEFAULT_VOLUME 1.0
#define DEFAULT_SPEED 1.0
#define DEFAULT_STATE NYX_PLAYER_STATE_STOPPED
#define DEFAULT_VIDEO_ENABLED TRUE
#define DEFAULT_AUDIO_ENABLED TRUE
#define DEFAULT_SUBTITLES_ENABLED TRUE
#define DEFAULT_DOWNLOAD_ENABLED FALSE
#define DEFAULT_ADAPTIVE_START_BITRATE 1600000

#define GST_CAT_DEFAULT nyx_player_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class nyx_player_parent_class
G_DEFINE_TYPE (NyxPlayer, nyx_player, NYX_TYPE_THREADED_OBJECT);

enum
{
  PROP_0,
  PROP_QUEUE,
  PROP_VIDEO_STREAMS,
  PROP_AUDIO_STREAMS,
  PROP_SUBTITLE_STREAMS,
  PROP_ENHANCER_PROXIES,
  PROP_AUTOPLAY,
  PROP_POSITION,
  PROP_SPEED,
  PROP_STATE,
  PROP_MUTE,
  PROP_VOLUME,
  PROP_VIDEO_SINK,
  PROP_AUDIO_SINK,
  PROP_VIDEO_FILTER,
  PROP_AUDIO_FILTER,
  PROP_CURRENT_VIDEO_DECODER,
  PROP_CURRENT_AUDIO_DECODER,
  PROP_VIDEO_ENABLED,
  PROP_AUDIO_ENABLED,
  PROP_SUBTITLES_ENABLED,
  PROP_DOWNLOAD_DIR,
  PROP_DOWNLOAD_ENABLED,
  PROP_ADAPTIVE_START_BITRATE,
  PROP_ADAPTIVE_MIN_BITRATE,
  PROP_ADAPTIVE_MAX_BITRATE,
  PROP_ADAPTIVE_BANDWIDTH,
  PROP_AUDIO_OFFSET,
  PROP_SUBTITLE_OFFSET,
  PROP_SUBTITLE_FONT_DESC,
  PROP_LAST
};

enum
{
  SIGNAL_SEEK_DONE,
  SIGNAL_DOWNLOAD_COMPLETE,
  SIGNAL_MISSING_PLUGIN,
  SIGNAL_MESSAGE,
  SIGNAL_WARNING,
  SIGNAL_ERROR,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

/* Properties we expose through API, thus we want to emit notify signals for them */
static const gchar *playbin_watchlist[] = {
  "volume",
  "mute",
  "flags",
  "audio-sink",
  "video-sink",
  "audio-filter",
  "video-filter",
  "av-offset",
  "text-offset",
  NULL
};

gboolean
nyx_player_refresh_position (NyxPlayer *self)
{
  gint64 position = GST_CLOCK_TIME_NONE;
  gdouble position_dbl;
  gboolean changed;

  if (gst_element_query (self->playbin, self->position_query))
    gst_query_parse_position (self->position_query, NULL, &position);

  if (position < 0)
    position = 0;

  position_dbl = (gdouble) position / GST_SECOND;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->position, position_dbl, FLT_EPSILON)))
    self->position = position_dbl;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_LOG_OBJECT (self, "Position: %" NYX_TIME_MS_FORMAT,
        NYX_TIME_MS_ARGS (position_dbl));

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_POSITION]);
    if (self->reactables_manager)
      nyx_reactables_manager_trigger_position_changed (self->reactables_manager, position_dbl);
    if (nyx_player_get_have_features (self))
      nyx_features_manager_trigger_position_changed (self->features_manager, position_dbl);
  }

  return G_SOURCE_CONTINUE;
}

void
nyx_player_add_tick_source (NyxPlayer *self)
{
  GST_OBJECT_LOCK (self);
  if (!self->tick_source) {
    self->tick_source = nyx_shared_utils_context_timeout_add_full (
        nyx_threaded_object_get_context (NYX_THREADED_OBJECT_CAST (self)),
        G_PRIORITY_DEFAULT_IDLE, 100,
        (GSourceFunc) nyx_player_refresh_position,
        self, NULL);
    GST_TRACE_OBJECT (self, "Added tick source");
  }
  GST_OBJECT_UNLOCK (self);
}

void
nyx_player_remove_tick_source (NyxPlayer *self)
{
  GST_OBJECT_LOCK (self);
  if (self->tick_source) {
    g_source_destroy (self->tick_source);
    g_clear_pointer (&self->tick_source, g_source_unref);
    GST_TRACE_OBJECT (self, "Removed tick source");
  }
  GST_OBJECT_UNLOCK (self);
}

void
nyx_player_handle_playbin_state_changed (NyxPlayer *self)
{
  NyxPlayerState state;
  gboolean changed;

  if (self->is_buffering) {
    state = NYX_PLAYER_STATE_BUFFERING;
  } else {
    switch (self->current_state) {
      case GST_STATE_PLAYING:
        state = NYX_PLAYER_STATE_PLAYING;
        break;
      case GST_STATE_PAUSED:
        state = NYX_PLAYER_STATE_PAUSED;
        break;
      default:
        state = NYX_PLAYER_STATE_STOPPED;
        break;
    }
  }

  GST_OBJECT_LOCK (self);
  if ((changed = self->state != state))
    self->state = state;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "State changed, now: %i", state);

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_STATE]);
    if (self->reactables_manager)
      nyx_reactables_manager_trigger_state_changed (self->reactables_manager, state);
    if (nyx_player_get_have_features (self))
      nyx_features_manager_trigger_state_changed (self->features_manager, state);
  }
}

/* Not using common_prop_changed() because needs linear -> cubic conversion
 * before applying and can only be applied during playback */
void
nyx_player_handle_playbin_volume_changed (NyxPlayer *self, const GValue *value)
{
  gdouble volume, volume_linear;
  gboolean changed;

  volume_linear = g_value_get_double (value);
  GST_DEBUG_OBJECT (self, "Playbin volume changed, linear: %lf", volume_linear);

  volume = gst_stream_volume_convert_volume (
      GST_STREAM_VOLUME_FORMAT_LINEAR,
      GST_STREAM_VOLUME_FORMAT_CUBIC,
      volume_linear);

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->volume, volume, FLT_EPSILON)))
    self->volume = volume;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Volume: %.2lf", volume);

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_VOLUME]);
    if (self->reactables_manager)
      nyx_reactables_manager_trigger_volume_changed (self->reactables_manager, volume);
    if (nyx_player_get_have_features (self))
      nyx_features_manager_trigger_volume_changed (self->features_manager, volume);
  }
}

/* Not using common_prop_changed() because can only be applied during playback */
void
nyx_player_handle_playbin_mute_changed (NyxPlayer *self, const GValue *value)
{
  gboolean mute, changed;

  mute = g_value_get_boolean (value);
  GST_DEBUG_OBJECT (self, "Playbin mute changed");

  GST_OBJECT_LOCK (self);
  if ((changed = self->mute != mute))
    self->mute = mute;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Mute: %s", (mute) ? "yes" : "no");

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_MUTE]);
    if (self->reactables_manager)
      nyx_reactables_manager_trigger_mute_changed (self->reactables_manager, mute);
    if (nyx_player_get_have_features (self))
      nyx_features_manager_trigger_mute_changed (self->features_manager, mute);
  }
}

void
nyx_player_handle_playbin_flags_changed (NyxPlayer *self, const GValue *value)
{
  gint flags;
  gboolean video_enabled, audio_enabled, subtitles_enabled, download_enabled;
  gboolean video_changed, audio_changed, subtitles_changed, download_changed;

  flags = g_value_get_flags (value);

  video_enabled = ((flags & NYX_PLAYER_PLAY_FLAG_VIDEO) == NYX_PLAYER_PLAY_FLAG_VIDEO);
  audio_enabled = ((flags & NYX_PLAYER_PLAY_FLAG_AUDIO) == NYX_PLAYER_PLAY_FLAG_AUDIO);
  subtitles_enabled = ((flags & NYX_PLAYER_PLAY_FLAG_TEXT) == NYX_PLAYER_PLAY_FLAG_TEXT);
  download_enabled = ((flags & NYX_PLAYER_PLAY_FLAG_DOWNLOAD) == NYX_PLAYER_PLAY_FLAG_DOWNLOAD);

  GST_OBJECT_LOCK (self);

  if ((video_changed = self->video_enabled != video_enabled))
    self->video_enabled = video_enabled;
  if ((audio_changed = self->audio_enabled != audio_enabled))
    self->audio_enabled = audio_enabled;
  if ((subtitles_changed = self->subtitles_enabled != subtitles_enabled))
    self->subtitles_enabled = subtitles_enabled;
  if ((download_changed = self->download_enabled != download_enabled))
    self->download_enabled = download_enabled;

  GST_OBJECT_UNLOCK (self);

  if (video_changed) {
    GST_INFO_OBJECT (self, "Video enabled: %s", (video_enabled) ? "yes" : "no");
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_VIDEO_ENABLED]);
  }
  if (audio_changed) {
    GST_INFO_OBJECT (self, "Audio enabled: %s", (audio_enabled) ? "yes" : "no");
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_AUDIO_ENABLED]);
  }
  if (subtitles_changed) {
    GST_INFO_OBJECT (self, "Subtitles enabled: %s", (subtitles_enabled) ? "yes" : "no");
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_SUBTITLES_ENABLED]);
  }
  if (download_changed) {
    GST_INFO_OBJECT (self, "Download enabled: %s", (download_enabled) ? "yes" : "no");
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_DOWNLOAD_ENABLED]);
  }
}

void
nyx_player_handle_playbin_av_offset_changed (NyxPlayer *self, const GValue *value)
{
  gdouble offset = (gdouble) g_value_get_int64 (value) / GST_SECOND;
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->audio_offset, offset, FLT_EPSILON)))
    self->audio_offset = offset;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Audio offset: %.2lf", offset);

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_AUDIO_OFFSET]);
  }
}

void
nyx_player_handle_playbin_text_offset_changed (NyxPlayer *self, const GValue *value)
{
  gdouble offset = (gdouble) g_value_get_int64 (value) / GST_SECOND;
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->subtitle_offset, offset, FLT_EPSILON)))
    self->subtitle_offset = offset;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Subtitles offset: %.2lf", offset);

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_SUBTITLE_OFFSET]);
  }
}

void
nyx_player_handle_playbin_common_prop_changed (NyxPlayer *self, const gchar *prop_name)
{
  GObjectClass *gobject_class = G_OBJECT_GET_CLASS (self);
  GParamSpec *pspec = g_object_class_find_property (gobject_class, prop_name);

  if (G_LIKELY (pspec != NULL)) {
    GST_DEBUG_OBJECT (self, "Playbin %s changed", prop_name);
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), pspec);
  }
}

void
nyx_player_handle_playbin_rate_changed (NyxPlayer *self, gdouble speed)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->speed, speed, FLT_EPSILON)))
    self->speed = speed;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Speed: %.2lf", speed);

    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_SPEED]);
    if (self->reactables_manager)
      nyx_reactables_manager_trigger_speed_changed (self->reactables_manager, speed);
    if (nyx_player_get_have_features (self))
      nyx_features_manager_trigger_speed_changed (self->features_manager, speed);
  }
}

static void
nyx_player_set_current_video_decoder (NyxPlayer *self, GstElement *element)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = gst_object_replace ((GstObject **) &self->video_decoder, GST_OBJECT_CAST (element));
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Current video decoder: %" GST_PTR_FORMAT, element);
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_VIDEO_DECODER]);
  }
}

static void
nyx_player_set_current_audio_decoder (NyxPlayer *self, GstElement *element)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = gst_object_replace ((GstObject **) &self->audio_decoder, GST_OBJECT_CAST (element));
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Current audio decoder: %" GST_PTR_FORMAT, element);
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_AUDIO_DECODER]);
  }
}

static void
nyx_player_set_pending_item_internal (NyxPlayer *self, NyxMediaItem *pending_item,
    NyxQueueItemChangeMode mode, gboolean load_suburi)
{
  const gchar *uri = NULL;
  gchar *suburi = NULL;
  gboolean has_suburi = FALSE;
  gboolean use_suburi = FALSE;

  /* We cannot do gapless/instant with pending suburi in place,
   * do a check and if necessary use normal mode instead */
  if (mode > NYX_QUEUE_ITEM_CHANGE_NORMAL) {
    g_object_get (self->playbin, "suburi", &suburi, NULL);

    if (suburi) {
      mode = NYX_QUEUE_ITEM_CHANGE_NORMAL;
      g_clear_pointer (&suburi, g_free);
    }
  }

  /* Might be NULL (e.g. after queue is cleared) */
  if (pending_item) {
    uri = nyx_media_item_get_playback_uri (pending_item);
    suburi = nyx_media_item_get_suburi (pending_item);
  }

  has_suburi = (suburi && *suburi);
  use_suburi = has_suburi && mode == NYX_QUEUE_ITEM_CHANGE_NORMAL
      && (load_suburi || !self->use_playbin3);

  GST_INFO_OBJECT (self, "Changing item with mode %u, URI: \"%s\", SUBURI: \"%s\"",
      mode, GST_STR_NULL (uri), GST_STR_NULL (suburi));

  /* We need to lock here, as this function is also called from "about-to-finish"
   * signal which comes from different thread and we need to change URIs in it ASAP,
   * so we cannot schedule an invoke of another thread there */
  GST_OBJECT_LOCK (self);
  gst_object_replace ((GstObject **) &self->pending_item, GST_OBJECT_CAST (pending_item));
  self->pending_suburi_reload = (self->use_playbin3 && has_suburi && !use_suburi);
  GST_OBJECT_UNLOCK (self);

  g_object_set (self->playbin, "suburi", (use_suburi) ? suburi : NULL, NULL);

  if (uri) {
    if (mode == NYX_QUEUE_ITEM_CHANGE_INSTANT)
      g_object_set (self->playbin, "instant-uri", TRUE, NULL);

    g_object_set (self->playbin, "uri", uri, NULL);

    if (mode == NYX_QUEUE_ITEM_CHANGE_INSTANT)
      g_object_set (self->playbin, "instant-uri", FALSE, NULL);
  }

  g_free (suburi);
}

void
nyx_player_set_pending_item (NyxPlayer *self, NyxMediaItem *pending_item,
    NyxQueueItemChangeMode mode)
{
  nyx_player_set_pending_item_internal (self, pending_item, mode, FALSE);
}

void
nyx_player_set_pending_item_with_suburi (NyxPlayer *self, NyxMediaItem *pending_item,
    NyxQueueItemChangeMode mode)
{
  nyx_player_set_pending_item_internal (self, pending_item, mode, TRUE);
}

static void
_stream_notify_cb (GstStreamCollection *collection,
    GstStream *gst_stream, GParamSpec *pspec, NyxPlayer *self)
{
  GstStreamType stream_type;
  NyxStream *stream = NULL;
  const gchar *pspec_name = g_param_spec_get_name (pspec);
  GstCaps *caps = NULL;
  GstTagList *tags = NULL;

  if (pspec_name == g_intern_string ("caps"))
    caps = gst_stream_get_caps (gst_stream);
  else if (pspec_name == g_intern_string ("tags"))
    tags = gst_stream_get_tags (gst_stream);
  else
    return;

  stream_type = gst_stream_get_stream_type (gst_stream);

  if ((stream_type & GST_STREAM_TYPE_VIDEO) == GST_STREAM_TYPE_VIDEO) {
    stream = nyx_stream_list_get_stream_for_gst_stream (self->video_streams, gst_stream);
  } else if ((stream_type & GST_STREAM_TYPE_AUDIO) == GST_STREAM_TYPE_AUDIO) {
    stream = nyx_stream_list_get_stream_for_gst_stream (self->audio_streams, gst_stream);
  } else if ((stream_type & GST_STREAM_TYPE_TEXT) == GST_STREAM_TYPE_TEXT) {
    stream = nyx_stream_list_get_stream_for_gst_stream (self->subtitle_streams, gst_stream);
  }

  if (G_LIKELY (stream != NULL)) {
    NyxStreamClass *stream_class = NYX_STREAM_GET_CLASS (stream);

    stream_class->internal_stream_updated (stream, caps, tags);
    gst_object_unref (stream);
  }

  gst_clear_caps (&caps);
  gst_clear_tag_list (&tags);
}

void
nyx_player_take_stream_collection (NyxPlayer *self, GstStreamCollection *collection)
{
  GST_OBJECT_LOCK (self);

  if (self->stream_notify_id != 0) {
    g_signal_handler_disconnect (self->collection, self->stream_notify_id);
    self->stream_notify_id = 0;
  }
  gst_clear_object (&self->collection);
  self->collection = collection;

  GST_OBJECT_UNLOCK (self);
}

/*
 * Must be called from main thread!
 */
void
nyx_player_refresh_streams (NyxPlayer *self)
{
  GList *vstreams = NULL, *astreams = NULL, *sstreams = NULL;
  guint i, n_streams;

  GST_TRACE_OBJECT (self, "Removing all obsolete streams");

  GST_OBJECT_LOCK (self);

  /* We should not be connected here anymore, but better be safe */
  if (G_LIKELY (self->stream_notify_id == 0)) {
    /* Initial update is done upon stream construction, thus
     * we do not have to call this callback here after connecting
     * (also why we connect it before constructing our streams). */
    self->stream_notify_id = g_signal_connect (self->collection, "stream-notify",
        G_CALLBACK (_stream_notify_cb), self);
  }

  n_streams = gst_stream_collection_get_size (self->collection);

  for (i = 0; i < n_streams; ++i) {
    GstStream *gst_stream = gst_stream_collection_get_stream (self->collection, i);
    GstStreamType stream_type = gst_stream_get_stream_type (gst_stream);

    GST_LOG_OBJECT (self, "Found %" GST_PTR_FORMAT, gst_stream);

    if ((stream_type & GST_STREAM_TYPE_VIDEO) == GST_STREAM_TYPE_VIDEO) {
      vstreams = g_list_append (vstreams, nyx_video_stream_new (gst_stream));
    } else if ((stream_type & GST_STREAM_TYPE_AUDIO) == GST_STREAM_TYPE_AUDIO) {
      astreams = g_list_append (astreams, nyx_audio_stream_new (gst_stream));
    } else if ((stream_type & GST_STREAM_TYPE_TEXT) == GST_STREAM_TYPE_TEXT) {
      sstreams = g_list_append (sstreams, nyx_subtitle_stream_new (gst_stream));
    } else {
      GST_WARNING_OBJECT (self, "Unhandled stream type: %s",
          gst_stream_type_get_name (stream_type));
    }
  }

  GST_OBJECT_UNLOCK (self);

  nyx_stream_list_replace_streams (self->video_streams, vstreams);
  nyx_stream_list_replace_streams (self->audio_streams, astreams);
  nyx_stream_list_replace_streams (self->subtitle_streams, sstreams);

  /* We only want to do this once for all stream lists, so
   * playbin will select the same streams as we initially did */
  nyx_playbin_bus_post_stream_change (self->bus);

  if (vstreams)
    g_list_free (vstreams);
  if (astreams)
    g_list_free (astreams);
  if (sstreams)
    g_list_free (sstreams);
}

static gboolean
_iterate_decoder_pads (NyxPlayer *self, GstElement *element,
    const gchar *stream_id, GstElementFactoryListType type)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean found = FALSE;

  iter = gst_element_iterate_src_pads (element);

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *decoder_pad = g_value_get_object (&value);
    gchar *decoder_sid = gst_pad_get_stream_id (decoder_pad);

    GST_DEBUG_OBJECT (self, "Decoder stream: %s", decoder_sid);

    if ((found = (g_strcmp0 (decoder_sid, stream_id) == 0))) {
      GST_DEBUG_OBJECT (self, "Found decoder for stream: %s", stream_id);

      if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO) == GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO)
        nyx_player_set_current_video_decoder (self, element);
      else if ((type & GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO) == GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO)
        nyx_player_set_current_audio_decoder (self, element);
    }

    g_free (decoder_sid);
    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

gboolean
nyx_player_find_active_decoder_with_stream_id (NyxPlayer *self,
    GstElementFactoryListType type, const gchar *stream_id)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean found = FALSE;

  GST_DEBUG_OBJECT (self, "Searching for decoder with stream: %s", stream_id);

  type |= GST_ELEMENT_FACTORY_TYPE_DECODER;
  iter = gst_bin_iterate_recurse (GST_BIN_CAST (self->playbin));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstElement *element = g_value_get_object (&value);
    GstElementFactory *factory = gst_element_get_factory (element);

    if (factory && gst_element_factory_list_is_type (factory, type))
      found = _iterate_decoder_pads (self, element, stream_id, type);

    g_value_unset (&value);

    if (found)
      break;
  }

  gst_iterator_free (iter);

  return found;
}

/* For playbin2 only */
void
nyx_player_playbin_update_current_decoders (NyxPlayer *self)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean found_video = FALSE, found_audio = FALSE;

  iter = gst_bin_iterate_all_by_element_factory_name (
      GST_BIN_CAST (self->playbin), "input-selector");

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstElement *element = g_value_get_object (&value);
    GstPad *active_pad;

    g_object_get (element, "active-pad", &active_pad, NULL);

    if (active_pad) {
      gchar *stream_id;

      stream_id = gst_pad_get_stream_id (active_pad);
      gst_object_unref (active_pad);

      if (stream_id) {
        if (!found_video) {
          found_video = nyx_player_find_active_decoder_with_stream_id (self,
              GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, stream_id);
        }
        if (!found_audio) {
          found_audio = nyx_player_find_active_decoder_with_stream_id (self,
              GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, stream_id);
        }
        g_free (stream_id);
      }
    }

    g_value_unset (&value);

    if (found_video && found_audio)
      break;
  }

  gst_iterator_free (iter);

  if (!found_video)
    GST_DEBUG_OBJECT (self, "Active video decoder not found");
  if (!found_audio)
    GST_DEBUG_OBJECT (self, "Active audio decoder not found");
}

static void
_adaptive_demuxer_bandwidth_changed_cb (GstElement *adaptive_demuxer,
    GParamSpec *pspec G_GNUC_UNUSED, NyxPlayer *self)
{
  guint bandwidth = 0;
  gboolean changed;

  g_object_get (adaptive_demuxer, "current-bandwidth", &bandwidth, NULL);

  /* Skip uncalculated bandwidth from
   * new adaptive demuxer instance */
  if (bandwidth == 0)
    return;

  GST_OBJECT_LOCK (self);
  if ((changed = bandwidth != self->bandwidth))
    self->bandwidth = bandwidth;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_LOG_OBJECT (self, "Adaptive bandwidth: %u", bandwidth);
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_ADAPTIVE_BANDWIDTH]);
  }
}

void
nyx_player_reset (NyxPlayer *self, gboolean pending_dispose)
{
  GST_OBJECT_LOCK (self);

  GST_DEBUG_OBJECT (self, "Reset");

  self->had_error = FALSE;
  self->pending_flush = FALSE;
  gst_clear_object (&self->played_item);

  if (pending_dispose) {
    gst_clear_object (&self->video_decoder);
    gst_clear_object (&self->audio_decoder);
  }

  if (self->adaptive_demuxer) {
    g_signal_handlers_disconnect_by_func (self->adaptive_demuxer,
        _adaptive_demuxer_bandwidth_changed_cb, self);
    gst_clear_object (&self->adaptive_demuxer);
  }

  GST_OBJECT_UNLOCK (self);

  self->stream_tags_allowed = FALSE;
  gst_clear_tag_list (&self->pending_tags);

  if (self->pending_toc) {
    gst_toc_unref (self->pending_toc);
    self->pending_toc = NULL;
  }

  /* Emit notify when we are not going to be disposed */
  if (!pending_dispose) {
    /* Clear current decoders (next item might not have video/audio track) */
    nyx_player_set_current_video_decoder (self, NULL);
    nyx_player_set_current_audio_decoder (self, NULL);
  }
}

static inline gchar *
_make_download_template (NyxPlayer *self)
{
  gchar *download_template = NULL;

  GST_OBJECT_LOCK (self);

  if (self->download_enabled && self->download_dir) {
    if (g_mkdir_with_parents (self->download_dir, 0755) == 0) {
      download_template = g_build_filename (self->download_dir, "XXXXXX", NULL);
    } else {
      GST_ERROR_OBJECT (self, "Could not create download dir: \"%s\"", self->download_dir);
    }
  }

  GST_OBJECT_UNLOCK (self);

  return download_template;
}

static void
_element_setup_cb (GstElement *playbin, GstElement *element, NyxPlayer *self)
{
  GstElementFactory *factory = gst_element_get_factory (element);
  const gchar *factory_name;

  if (G_UNLIKELY (factory == NULL))
    return;

  factory_name = g_intern_static_string (GST_OBJECT_NAME (factory));
  GST_INFO_OBJECT (self, "Element setup: %s", factory_name);

  if (factory_name == g_intern_static_string ("nyxextractablesrc")
      || factory_name == g_intern_static_string ("nyxplaylistdemux")) {
    g_object_set (element,
        "enhancer-proxies", self->enhancer_proxies,
        NULL);
  } else if (factory_name == g_intern_static_string ("downloadbuffer")) {
    gchar *download_template;

    /* Only set props if we have download template */
    if ((download_template = _make_download_template (self))) {
      g_object_set (element,
          "temp-template", download_template,
          "temp-remove", FALSE,
          NULL);
      g_free (download_template);
    }
  } else if (factory_name == g_intern_static_string ("dashdemux2")
      || factory_name == g_intern_static_string ("hlsdemux2")) {
    guint start_bitrate, min_bitrate, max_bitrate;

    GST_OBJECT_LOCK (self);

    start_bitrate = self->start_bitrate;
    min_bitrate = self->min_bitrate;
    max_bitrate = self->max_bitrate;

    if (self->adaptive_demuxer) {
      g_signal_handlers_disconnect_by_func (self->adaptive_demuxer,
          _adaptive_demuxer_bandwidth_changed_cb, self);
    }

    gst_object_replace ((GstObject **) &self->adaptive_demuxer, GST_OBJECT_CAST (element));

    if (self->adaptive_demuxer) {
      g_signal_connect (self->adaptive_demuxer, "notify::current-bandwidth",
          G_CALLBACK (_adaptive_demuxer_bandwidth_changed_cb), self);
    }

    GST_OBJECT_UNLOCK (self);

    g_object_set (element,
        "low-watermark-time", 3 * GST_SECOND,
        "high-watermark-time", 10 * GST_SECOND,
        "start-bitrate", start_bitrate,
        "min-bitrate", min_bitrate,
        "max-bitrate", max_bitrate,
        NULL);
  }
}

static void
_about_to_finish_cb (GstElement *playbin, NyxPlayer *self)
{
  gboolean had_error;

  GST_INFO_OBJECT (self, "About to finish");

  /* This signal comes from different thread */
  GST_OBJECT_LOCK (self);
  had_error = self->had_error;
  GST_OBJECT_UNLOCK (self);

  /* We do not want to progress playlist after error */
  if (G_UNLIKELY (had_error))
    return;

  nyx_queue_handle_about_to_finish (self->queue, self);
}

static void
_playbin_streams_changed_cb (GstElement *playbin, NyxPlayer *self)
{
  GstStreamCollection *collection = gst_stream_collection_new (NULL);
  gint i;

  GST_DEBUG_OBJECT (self, "Playbin streams changed");

  g_object_get (playbin, "n-video", &self->n_video, NULL);
  for (i = 0; i < self->n_video; ++i) {
    gst_stream_collection_add_stream (collection,
        gst_stream_new (NULL, NULL, GST_STREAM_TYPE_VIDEO, GST_STREAM_FLAG_NONE));
  }

  g_object_get (playbin, "n-audio", &self->n_audio, NULL);
  for (i = 0; i < self->n_audio; ++i) {
    gst_stream_collection_add_stream (collection,
        gst_stream_new (NULL, NULL, GST_STREAM_TYPE_AUDIO, GST_STREAM_FLAG_NONE));
  }

  g_object_get (playbin, "n-text", &self->n_text, NULL);
  for (i = 0; i < self->n_text; ++i) {
    gst_stream_collection_add_stream (collection,
        gst_stream_new (NULL, NULL, GST_STREAM_TYPE_TEXT, GST_STREAM_FLAG_NONE));
  }

  nyx_player_take_stream_collection (self, collection);
}

static void
_playbin_tags_changed (NyxPlayer *self, gint index, gint global_index)
{
  GstStream *gst_stream;
  GstStreamType stream_type;
  GstTagList *tags = NULL;
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  gst_stream = gst_stream_collection_get_stream (self->collection, global_index);
  stream_type = gst_stream_get_stream_type (gst_stream);

  if ((stream_type & GST_STREAM_TYPE_VIDEO) == GST_STREAM_TYPE_VIDEO) {
    g_signal_emit_by_name (self->playbin, "get-video-tags", index, &tags);
    g_signal_emit_by_name (self->playbin, "get-video-pad", index, &pad);
  } else if ((stream_type & GST_STREAM_TYPE_AUDIO) == GST_STREAM_TYPE_AUDIO) {
    g_signal_emit_by_name (self->playbin, "get-audio-tags", index, &tags);
    g_signal_emit_by_name (self->playbin, "get-audio-pad", index, &pad);
  } else if ((stream_type & GST_STREAM_TYPE_TEXT) == GST_STREAM_TYPE_TEXT) {
    g_signal_emit_by_name (self->playbin, "get-text-tags", index, &tags);
    g_signal_emit_by_name (self->playbin, "get-text-pad", index, &pad);
  }

  gst_stream_set_tags (gst_stream, tags);
  gst_clear_tag_list (&tags);

  if (G_LIKELY (pad != NULL)) {
    caps = gst_pad_get_current_caps (pad);
    gst_object_unref (pad);
  }

  gst_stream_set_caps (gst_stream, caps);
  gst_clear_caps (&caps);
}

static void
_playbin_video_tags_changed_cb (GstElement *playbin, gint index, NyxPlayer *self)
{
  GST_DEBUG_OBJECT (self, "Video stream %i tags changed", index);
  _playbin_tags_changed (self, index, index);
}

static void
_playbin_audio_tags_changed_cb (GstElement *playbin, gint index, NyxPlayer *self)
{
  GST_DEBUG_OBJECT (self, "Audio stream %i tags changed", index);
  _playbin_tags_changed (self, index, self->n_video + index);
}

static void
_playbin_text_tags_changed_cb (GstElement *playbin, gint index, NyxPlayer *self)
{
  GST_DEBUG_OBJECT (self, "Subtitle stream %i tags changed", index);
  _playbin_tags_changed (self, index, self->n_video + self->n_audio + index);
}

static void
_playbin_selected_streams_changed_cb (GstElement *playbin,
    GParamSpec *pspec G_GNUC_UNUSED, NyxPlayer *self)
{
  GstMessage *msg;
  gint current_video = 0, current_audio = 0, current_text = 0;
  gboolean success = TRUE;

  msg = gst_message_new_streams_selected (
      GST_OBJECT_CAST (playbin), self->collection);

  g_object_get (playbin,
      "current-video", &current_video,
      "current-audio", &current_audio,
      "current-text", &current_text, NULL);

  GST_DEBUG_OBJECT (self, "Selected streams changed, video: %i, audio: %i, text: %i",
      current_video, current_audio, current_text);

  /* We cannot play text stream only, skip streams selected for now */
  if (current_video < 0 && current_audio < 0) {
    success = FALSE;
    goto finish;
  }

  if (current_video >= 0) {
    GstStream *gst_stream = gst_stream_collection_get_stream (self->collection,
        current_video);

    if (gst_stream)
      gst_message_streams_selected_add (msg, gst_stream);
    else
      success = FALSE;
  }
  if (current_audio >= 0) {
    GstStream *gst_stream = gst_stream_collection_get_stream (self->collection,
        self->n_video + current_audio);

    if (gst_stream)
      gst_message_streams_selected_add (msg, gst_stream);
    else
      success = FALSE;
  }
  if (current_text >= 0) {
    GstStream *gst_stream = gst_stream_collection_get_stream (self->collection,
        self->n_video + self->n_audio + current_text);

    if (gst_stream)
      gst_message_streams_selected_add (msg, gst_stream);
    else
      success = FALSE;
  }

finish:
  /* Since "current-*" is changed one at a time from signal emissions,
   * we might fail here to assemble everything until last signal */
  if (success)
    gst_bus_post (self->bus, msg);
  else
    gst_message_unref (msg);
}

NyxPlayer *
nyx_player_get_from_ancestor (GstObject *object)
{
  GstObject *parent = gst_object_get_parent (object);

  while (parent) {
    GstObject *tmp;

    if (NYX_IS_PLAYER (parent))
      return NYX_PLAYER_CAST (parent);

    tmp = gst_object_get_parent (parent);
    gst_object_unref (parent);
    parent = tmp;
  }

  return NULL;
}

/**
 * nyx_player_new:
 *
 * Creates a new #NyxPlayer instance.
 *
 * Returns: (transfer full): a new #NyxPlayer instance.
 */
NyxPlayer *
nyx_player_new (void)
{
  NyxPlayer *player;

  player = g_object_new (NYX_TYPE_PLAYER, NULL);
  gst_object_ref_sink (player);

  return player;
}

/**
 * nyx_player_get_queue:
 * @player: a #NyxPlayer
 *
 * Get the #NyxQueue of the player.
 *
 * The queue belongs to the player and can be accessed for as long
 * as #NyxPlayer object instance it belongs to is alive.
 *
 * Returns: (transfer none): the #NyxQueue of the player.
 */
NyxQueue *
nyx_player_get_queue (NyxPlayer *self)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return self->queue;
}

/**
 * nyx_player_get_video_streams:
 * @player: a #NyxPlayer
 *
 * Get a list of video streams within media item.
 *
 * Returns: (transfer none): a #NyxStreamList of video #NyxStream.
 */
NyxStreamList *
nyx_player_get_video_streams (NyxPlayer *self)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return self->video_streams;
}

/**
 * nyx_player_get_audio_streams:
 * @player: a #NyxPlayer
 *
 * Get a list of audio streams within media item.
 *
 * Returns: (transfer none): a #NyxStreamList of audio #NyxStream.
 */
NyxStreamList *
nyx_player_get_audio_streams (NyxPlayer *self)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return self->audio_streams;
}

/**
 * nyx_player_get_subtitle_streams:
 * @player: a #NyxPlayer
 *
 * Get a list of subtitle streams within media item.
 *
 * Returns: (transfer none): a #NyxStreamList of subtitle #NyxStream.
 */
NyxStreamList *
nyx_player_get_subtitle_streams (NyxPlayer *self)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return self->subtitle_streams;
}

/**
 * nyx_player_get_enhancer_proxies:
 * @player: a #NyxPlayer
 *
 * Get a list of available enhancers in the form of [class@Nyx.EnhancerProxy] objects.
 *
 * Returns: (transfer none): a #NyxEnhancerProxyList of enhancer proxies.
 *
 * Since: 0.10
 */
NyxEnhancerProxyList *
nyx_player_get_enhancer_proxies (NyxPlayer *self)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return self->enhancer_proxies;
}

/**
 * nyx_player_set_autoplay:
 * @player: a #NyxPlayer
 * @enabled: %TRUE to enable autoplay, %FALSE otherwise.
 *
 * Set the autoplay state of the player.
 *
 * When autoplay is enabled, player will always try to start
 * playback after current media item changes. When disabled
 * current playback state is preserved when changing items.
 */
void
nyx_player_set_autoplay (NyxPlayer *self, gboolean autoplay)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_PLAYER (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->autoplay != autoplay))
    self->autoplay = autoplay;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_AUTOPLAY]);
  }
}

/**
 * nyx_player_get_autoplay:
 * @player: a #NyxPlayer
 *
 * Get the autoplay value.
 *
 * Returns: %TRUE if autoplay is enabled, %FALSE otherwise.
 */
gboolean
nyx_player_get_autoplay (NyxPlayer *self)
{
  gboolean autoplay;

  g_return_val_if_fail (NYX_IS_PLAYER (self), DEFAULT_AUTOPLAY);

  GST_OBJECT_LOCK (self);
  autoplay = self->autoplay;
  GST_OBJECT_UNLOCK (self);

  return autoplay;
}

/**
 * nyx_player_get_position:
 * @player: a #NyxPlayer
 *
 * Get the current player playback position.
 *
 * The returned value is in seconds as a decimal number.
 *
 * Returns: the position of the player.
 */
gdouble
nyx_player_get_position (NyxPlayer *self)
{
  gdouble position;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  position = self->position;
  GST_OBJECT_UNLOCK (self);

  return position;
}

/**
 * nyx_player_get_state:
 * @player: a #NyxPlayer
 *
 * Get the current #NyxPlayerState.
 *
 * Returns: the #NyxPlayerState of the player.
 */
NyxPlayerState
nyx_player_get_state (NyxPlayer *self)
{
  NyxPlayerState state;

  g_return_val_if_fail (NYX_IS_PLAYER (self), DEFAULT_STATE);

  GST_OBJECT_LOCK (self);
  state = self->state;
  GST_OBJECT_UNLOCK (self);

  return state;
}

/**
 * nyx_player_set_mute:
 * @player: a #NyxPlayer
 * @mute: %TRUE if player should be muted, %FALSE otherwise.
 *
 * Set the mute state of the player.
 */
void
nyx_player_set_mute (NyxPlayer *self, gboolean mute)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (NYX_IS_PLAYER (self));

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, mute);

  nyx_playbin_bus_post_set_prop (self->bus, GST_OBJECT_CAST (self->playbin), "mute", &value);
}

/**
 * nyx_player_get_mute:
 * @player: a #NyxPlayer
 *
 * Get the mute state of the player.
 *
 * Returns: %TRUE if player is muted, %FALSE otherwise.
 */
gboolean
nyx_player_get_mute (NyxPlayer *self)
{
  gboolean mute;

  g_return_val_if_fail (NYX_IS_PLAYER (self), DEFAULT_MUTE);

  GST_OBJECT_LOCK (self);
  mute = self->mute;
  GST_OBJECT_UNLOCK (self);

  return mute;
}

/**
 * nyx_player_set_volume:
 * @player: a #NyxPlayer
 * @volume: the volume level.
 *
 * Set the volume of the player.
 *
 * The value should be within 0 - 2.0 range, where 1.0 is 100%
 * volume and anything above results with an overamplification.
 */
void
nyx_player_set_volume (NyxPlayer *self, gdouble volume)
{
  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (volume >= 0 && volume <= 2.0);

  nyx_playbin_bus_post_set_volume (self->bus, self->playbin, volume);
}

/**
 * nyx_player_get_volume:
 * @player: a #NyxPlayer
 *
 * Get the volume of the player.
 *
 * Returns: current volume level.
 */
gdouble
nyx_player_get_volume (NyxPlayer *self)
{
  gdouble volume;

  g_return_val_if_fail (NYX_IS_PLAYER (self), DEFAULT_VOLUME);

  GST_OBJECT_LOCK (self);
  volume = self->volume;
  GST_OBJECT_UNLOCK (self);

  return volume;
}

/**
 * nyx_player_set_speed:
 * @player: a #NyxPlayer
 * @speed: the playback speed multiplier.
 *
 * Set the speed multiplier of the player.
 */
void
nyx_player_set_speed (NyxPlayer *self, gdouble speed)
{
  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (speed != 0);

  nyx_playbin_bus_post_rate_change (self->bus, speed);
}

/**
 * nyx_player_get_speed:
 * @player: a #NyxPlayer
 *
 * Get the speed of the player used for playback.
 *
 * Returns: the playback speed multiplier.
 */
gdouble
nyx_player_get_speed (NyxPlayer *self)
{
  gdouble speed;

  g_return_val_if_fail (NYX_IS_PLAYER (self), DEFAULT_SPEED);

  GST_OBJECT_LOCK (self);
  speed = self->speed;
  GST_OBJECT_UNLOCK (self);

  return speed;
}

/* XXX: Also serialized into player thread, so action order like stop() -> set_sink() -> play() is not racy */
static void
nyx_player_set_playbin_prop_element (NyxPlayer *self, const gchar *prop_name, GstElement *element)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (element == NULL || GST_IS_ELEMENT (element));

  g_value_init (&value, GST_TYPE_ELEMENT);
  g_value_set_object (&value, element);

  nyx_playbin_bus_post_set_prop (self->bus, GST_OBJECT_CAST (self->playbin), prop_name, &value);
}

static GstElement *
nyx_player_get_playbin_prop_element (NyxPlayer *self, const gchar *prop_name)
{
  GstElement *element = NULL;

  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  g_object_get (self->playbin, prop_name, &element, NULL);

  return element;
}

/**
 * nyx_player_set_video_sink:
 * @player: a #NyxPlayer
 * @element: (nullable): a #GstElement or %NULL to use default.
 *
 * Set #GstElement to be used as video sink.
 */
void
nyx_player_set_video_sink (NyxPlayer *self, GstElement *element)
{
  nyx_player_set_playbin_prop_element (self, "video-sink", element);
}

/**
 * nyx_player_get_video_sink:
 * @player: a #NyxPlayer
 *
 * Get #GstElement used as video sink.
 *
 * Returns: (transfer full): #GstElement set as video sink.
 */
GstElement *
nyx_player_get_video_sink (NyxPlayer *self)
{
  return nyx_player_get_playbin_prop_element (self, "video-sink");
}

/**
 * nyx_player_set_audio_sink:
 * @player: a #NyxPlayer
 * @element: (nullable): a #GstElement or %NULL to use default.
 *
 * Set #GstElement to be used as audio sink.
 */
void
nyx_player_set_audio_sink (NyxPlayer *self, GstElement *element)
{
  nyx_player_set_playbin_prop_element (self, "audio-sink", element);
}

/**
 * nyx_player_get_audio_sink:
 * @player: a #NyxPlayer
 *
 * Get #GstElement used as audio sink.
 *
 * Returns: (transfer full): #GstElement set as audio sink.
 */
GstElement *
nyx_player_get_audio_sink (NyxPlayer *self)
{
  return nyx_player_get_playbin_prop_element (self, "audio-sink");
}

/**
 * nyx_player_set_video_filter:
 * @player: a #NyxPlayer
 * @element: (nullable): a #GstElement or %NULL for none.
 *
 * Set #GstElement to be used as video filter.
 */
void
nyx_player_set_video_filter (NyxPlayer *self, GstElement *element)
{
  nyx_player_set_playbin_prop_element (self, "video-filter", element);
}

/**
 * nyx_player_get_video_filter:
 * @player: a #NyxPlayer
 *
 * Get #GstElement used as video filter.
 *
 * Returns: (transfer full): #GstElement set as video filter.
 */
GstElement *
nyx_player_get_video_filter (NyxPlayer *self)
{
  return nyx_player_get_playbin_prop_element (self, "video-filter");
}

/**
 * nyx_player_set_audio_filter:
 * @player: a #NyxPlayer
 * @element: (nullable): a #GstElement or %NULL for none.
 *
 * Set #GstElement to be used as audio filter.
 */
void
nyx_player_set_audio_filter (NyxPlayer *self, GstElement *element)
{
  nyx_player_set_playbin_prop_element (self, "audio-filter", element);
}

/**
 * nyx_player_get_audio_filter:
 * @player: a #NyxPlayer
 *
 * Get #GstElement used as audio filter.
 *
 * Returns: (transfer full): #GstElement set as audio filter.
 */
GstElement *
nyx_player_get_audio_filter (NyxPlayer *self)
{
  return nyx_player_get_playbin_prop_element (self, "audio-filter");
}

/**
 * nyx_player_get_current_video_decoder:
 * @player: a #NyxPlayer
 *
 * Get #GstElement currently used as video decoder.
 *
 * Returns: (transfer full): #GstElement currently used as video decoder.
 */
GstElement *
nyx_player_get_current_video_decoder (NyxPlayer *self)
{
  GstElement *element = NULL;

  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  GST_OBJECT_LOCK (self);
  if (self->video_decoder)
    element = gst_object_ref (self->video_decoder);
  GST_OBJECT_UNLOCK (self);

  return element;
}

/**
 * nyx_player_get_current_audio_decoder:
 * @player: a #NyxPlayer
 *
 * Get #GstElement currently used as audio decoder.
 *
 * Returns: (transfer full): #GstElement currently used as audio decoder.
 */
GstElement *
nyx_player_get_current_audio_decoder (NyxPlayer *self)
{
  GstElement *element = NULL;

  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  GST_OBJECT_LOCK (self);
  if (self->audio_decoder)
    element = gst_object_ref (self->audio_decoder);
  GST_OBJECT_UNLOCK (self);

  return element;
}

/**
 * nyx_player_set_video_enabled:
 * @player: a #NyxPlayer
 * @enabled: whether enabled
 *
 * Set whether enable video stream.
 */
void
nyx_player_set_video_enabled (NyxPlayer *self, gboolean enabled)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_set_play_flag (self->bus, NYX_PLAYER_PLAY_FLAG_VIDEO, enabled);
}

/**
 * nyx_player_get_video_enabled:
 * @player: a #NyxPlayer
 *
 * Get whether video stream is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
nyx_player_get_video_enabled (NyxPlayer *self)
{
  gboolean enabled;

  g_return_val_if_fail (NYX_IS_PLAYER (self), FALSE);

  GST_OBJECT_LOCK (self);
  enabled = self->video_enabled;
  GST_OBJECT_UNLOCK (self);

  return enabled;
}

/**
 * nyx_player_set_audio_enabled:
 * @player: a #NyxPlayer
 * @enabled: whether enabled
 *
 * Set whether enable audio stream.
 */
void
nyx_player_set_audio_enabled (NyxPlayer *self, gboolean enabled)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_set_play_flag (self->bus, NYX_PLAYER_PLAY_FLAG_AUDIO, enabled);
}

/**
 * nyx_player_get_audio_enabled:
 * @player: a #NyxPlayer
 *
 * Get whether audio stream is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
nyx_player_get_audio_enabled (NyxPlayer *self)
{
  gboolean enabled;

  g_return_val_if_fail (NYX_IS_PLAYER (self), FALSE);

  GST_OBJECT_LOCK (self);
  enabled = self->audio_enabled;
  GST_OBJECT_UNLOCK (self);

  return enabled;
}

/**
 * nyx_player_set_subtitles_enabled:
 * @player: a #NyxPlayer
 * @enabled: whether enabled
 *
 * Set whether subtitles should be shown if any.
 */
void
nyx_player_set_subtitles_enabled (NyxPlayer *self, gboolean enabled)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_set_play_flag (self->bus, NYX_PLAYER_PLAY_FLAG_TEXT, enabled);
}

/**
 * nyx_player_get_subtitles_enabled:
 * @player: a #NyxPlayer
 *
 * Get whether subtitles are to be shown when available.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
nyx_player_get_subtitles_enabled (NyxPlayer *self)
{
  gboolean enabled;

  g_return_val_if_fail (NYX_IS_PLAYER (self), FALSE);

  GST_OBJECT_LOCK (self);
  enabled = self->subtitles_enabled;
  GST_OBJECT_UNLOCK (self);

  return enabled;
}

/**
 * nyx_player_set_download_dir:
 * @player: a #NyxPlayer
 * @path: (type filename): the path of a directory to use for media downloads
 *
 * Set a directory that @player will use to store downloads.
 *
 * See [property@Nyx.Player:download-enabled] description for more
 * info how this works.
 *
 * Since: 0.8
 */
void
nyx_player_set_download_dir (NyxPlayer *self, const gchar *path)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (path != NULL);

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->download_dir, path);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Current download dir: %s", path);
    nyx_app_bus_post_prop_notify (self->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_DOWNLOAD_DIR]);
  }
}

/**
 * nyx_player_get_download_dir:
 * @player: a #NyxPlayer
 *
 * Get path to a directory set for media downloads.
 *
 * Returns: (type filename) (transfer full) (nullable): the path of a directory
 *   set for media downloads or %NULL if no directory was set yet.
 *
 * Since: 0.8
 */
gchar *
nyx_player_get_download_dir (NyxPlayer *self)
{
  gchar *download_dir;

  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  GST_OBJECT_LOCK (self);
  download_dir = g_strdup (self->download_dir);
  GST_OBJECT_UNLOCK (self);

  return download_dir;
}

/**
 * nyx_player_set_download_enabled:
 * @player: a #NyxPlayer
 * @enabled: whether enabled
 *
 * Set whether player should attempt progressive download buffering.
 *
 * For this to actually work a [property@Nyx.Player:download-dir]
 * must also be set.
 *
 * Since: 0.8
 */
void
nyx_player_set_download_enabled (NyxPlayer *self, gboolean enabled)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_set_play_flag (self->bus, NYX_PLAYER_PLAY_FLAG_DOWNLOAD, enabled);
}

/**
 * nyx_player_get_download_enabled:
 * @player: a #NyxPlayer
 *
 * Get whether progressive download buffering is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
nyx_player_get_download_enabled (NyxPlayer *self)
{
  gboolean enabled;

  g_return_val_if_fail (NYX_IS_PLAYER (self), FALSE);

  GST_OBJECT_LOCK (self);
  enabled = self->download_enabled;
  GST_OBJECT_UNLOCK (self);

  return enabled;
}

static void
_set_adaptive_bitrate (NyxPlayer *self, guint *internal_ptr,
    const gchar *prop_name, guint bitrate, GParamSpec *pspec)
{
  GstElement *element = NULL;
  gboolean changed;

  if (!self->use_playbin3) {
    GST_WARNING_OBJECT (self, "Setting adaptive-%s when using playbin2"
        " has no effect", prop_name);
  }

  GST_OBJECT_LOCK (self);
  if ((changed = (*internal_ptr != bitrate))) {
    *internal_ptr = bitrate;

    if (self->adaptive_demuxer)
      element = gst_object_ref (self->adaptive_demuxer);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Set adaptive-%s: %u", prop_name, bitrate);

    if (element)
      g_object_set (element, prop_name, bitrate, NULL);

    nyx_app_bus_post_prop_notify (self->app_bus, GST_OBJECT_CAST (self), pspec);
  }

  gst_clear_object (&element);
}

/**
 * nyx_player_set_adaptive_start_bitrate:
 * @player: a #NyxPlayer
 * @bitrate: a bitrate to set (bits/s)
 *
 * Set initial bitrate to select when starting adaptive
 * streaming such as DASH or HLS.
 *
 * Since: 0.8
 */
void
nyx_player_set_adaptive_start_bitrate (NyxPlayer *self, guint bitrate)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  _set_adaptive_bitrate (self, &self->start_bitrate,
      "start-bitrate", bitrate, param_specs[PROP_ADAPTIVE_START_BITRATE]);
}

/**
 * nyx_player_get_adaptive_start_bitrate:
 * @player: a #NyxPlayer
 *
 * Get currently set initial bitrate (bits/s) for adaptive streaming.
 *
 * Returns: the start bitrate value.
 *
 * Since: 0.8
 */
guint
nyx_player_get_adaptive_start_bitrate (NyxPlayer *self)
{
  guint bitrate;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->start_bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * nyx_player_set_adaptive_min_bitrate:
 * @player: a #NyxPlayer
 * @bitrate: a bitrate to set (bits/s)
 *
 * Set minimal bitrate to select for adaptive streaming
 * such as DASH or HLS.
 *
 * Since: 0.8
 */
void
nyx_player_set_adaptive_min_bitrate (NyxPlayer *self, guint bitrate)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  _set_adaptive_bitrate (self, &self->min_bitrate,
      "min-bitrate", bitrate, param_specs[PROP_ADAPTIVE_MIN_BITRATE]);
}

/**
 * nyx_player_get_adaptive_min_bitrate:
 * @player: a #NyxPlayer
 *
 * Get currently set minimal bitrate (bits/s) for adaptive streaming.
 *
 * Returns: the minimal bitrate value.
 *
 * Since: 0.8
 */
guint
nyx_player_get_adaptive_min_bitrate (NyxPlayer *self)
{
  guint bitrate;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->min_bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * nyx_player_set_adaptive_max_bitrate:
 * @player: a #NyxPlayer
 * @bitrate: a bitrate to set (bits/s)
 *
 * Set maximal bitrate to select for adaptive streaming
 * such as DASH or HLS.
 *
 * Since: 0.8
 */
void
nyx_player_set_adaptive_max_bitrate (NyxPlayer *self, guint bitrate)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  _set_adaptive_bitrate (self, &self->max_bitrate,
      "max-bitrate", bitrate, param_specs[PROP_ADAPTIVE_MAX_BITRATE]);
}

/**
 * nyx_player_get_adaptive_max_bitrate:
 * @player: a #NyxPlayer
 *
 * Get currently set maximal bitrate (bits/s) for adaptive streaming.
 *
 * Returns: the maximal bitrate value.
 *
 * Since: 0.8
 */
guint
nyx_player_get_adaptive_max_bitrate (NyxPlayer *self)
{
  guint bitrate;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->max_bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * nyx_player_get_adaptive_bandwidth:
 * @player: a #NyxPlayer
 *
 * Get last fragment download bandwidth (bits/s) during
 * adaptive streaming.
 *
 * Returns: the adaptive bandwidth.
 *
 * Since: 0.8
 */
guint
nyx_player_get_adaptive_bandwidth (NyxPlayer *self)
{
  guint bandwidth;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  bandwidth = self->bandwidth;
  GST_OBJECT_UNLOCK (self);

  return bandwidth;
}

/**
 * nyx_player_set_audio_offset:
 * @player: a #NyxPlayer
 * @offset: a decimal audio offset (in seconds)
 *
 * Set synchronisation offset between the audio stream and video.
 *
 * Positive values make the audio ahead of the video and negative
 * values make the audio go behind the video.
 */
void
nyx_player_set_audio_offset (NyxPlayer *self, gdouble offset)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (offset >= G_MININT64 && offset <= G_MAXINT64);

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, (gint64) (offset * GST_SECOND));

  nyx_playbin_bus_post_set_prop (self->bus,
      GST_OBJECT_CAST (self->playbin), "av-offset", &value);
}

/**
 * nyx_player_get_audio_offset:
 * @player: a #NyxPlayer
 *
 * Get the currently set audio stream offset.
 *
 * The returned value is in seconds as a decimal number.
 *
 * Returns: the audio stream offset.
 */
gdouble
nyx_player_get_audio_offset (NyxPlayer *self)
{
  gdouble offset;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  offset = self->audio_offset;
  GST_OBJECT_UNLOCK (self);

  return offset;
}

/**
 * nyx_player_set_subtitle_offset:
 * @player: a #NyxPlayer
 * @offset: a decimal subtitle stream offset (in seconds)
 *
 * Set synchronisation offset between the subtitle stream and video.
 *
 * Positive values make the subtitles ahead of the video and negative
 * values make the subtitles go behind the video.
 */
void
nyx_player_set_subtitle_offset (NyxPlayer *self, gdouble offset)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (offset >= G_MININT64 && offset <= G_MAXINT64);

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, (gint64) (offset * GST_SECOND));

  nyx_playbin_bus_post_set_prop (self->bus,
      GST_OBJECT_CAST (self->playbin), "text-offset", &value);
}

/**
 * nyx_player_get_subtitle_offset:
 * @player: a #NyxPlayer
 *
 * Get the currently set subtitle stream offset.
 *
 * The returned value is in seconds as a decimal number.
 *
 * Returns: the subtitle stream offset.
 */
gdouble
nyx_player_get_subtitle_offset (NyxPlayer *self)
{
  gdouble offset;

  g_return_val_if_fail (NYX_IS_PLAYER (self), 0);

  GST_OBJECT_LOCK (self);
  offset = self->subtitle_offset;
  GST_OBJECT_UNLOCK (self);

  return offset;
}

/**
 * nyx_player_set_subtitle_font_desc:
 * @player: a #NyxPlayer
 * @font_desc: Font description
 *
 * Set Pango font description to be used for subtitle stream rendering.
 */
void
nyx_player_set_subtitle_font_desc (NyxPlayer *self, const gchar *font_desc)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (NYX_IS_PLAYER (self));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, font_desc);

  nyx_playbin_bus_post_set_prop (self->bus,
      GST_OBJECT_CAST (self->playbin), "subtitle-font-desc", &value);
}

/**
 * nyx_player_get_subtitle_font_desc:
 * @player: a #NyxPlayer
 *
 * Get the currently set font description used for subtitle stream rendering.
 *
 * Returns: (transfer full): the subtitle font description.
 */
gchar *
nyx_player_get_subtitle_font_desc (NyxPlayer *self)
{
  gchar *font_desc = NULL;

  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  g_object_get (self->playbin, "subtitle-font-desc", &font_desc, NULL);

  return font_desc;
}

/**
 * nyx_player_play:
 * @player: a #NyxPlayer
 *
 * Either start or resume the playback of current media item.
 *
 * This function will queue a request for the underlaying #GStreamer
 * pipeline to go into `PLAYING` state.
 */
void
nyx_player_play (NyxPlayer *self)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_request_state (self->bus, self, GST_STATE_PLAYING);
}

/**
 * nyx_player_pause:
 * @player: a #NyxPlayer
 *
 * Pause the playback of current media item.
 *
 * This function will queue a request for the underlaying #GStreamer
 * pipeline to go into `PAUSED` state, thus can also be used on a not
 * yet started video to go into `PAUSED` state first.
 */
void
nyx_player_pause (NyxPlayer *self)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_request_state (self->bus, self, GST_STATE_PAUSED);
}

/**
 * nyx_player_stop:
 * @player: a #NyxPlayer
 *
 * Stop the playback of current media item.
 *
 * This function will queue a request for the underlaying #GStreamer
 * pipeline to go into `READY` state.
 */
void
nyx_player_stop (NyxPlayer *self)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_request_state (self->bus, self, GST_STATE_READY);
}

/**
 * nyx_player_seek:
 * @player: a #NyxPlayer
 * @position: a decimal number with position to seek to (in seconds)
 *
 * Request the player to perform a seek operation.
 *
 * This function will use [enum@Nyx.PlayerSeekMethod.NORMAL] as a
 * seeking method. If you wish to specify what method to use per seeking
 * request, use [method@Nyx.Player.seek_custom] instead.
 *
 * Note that seeking requests are per selected media item. Seeking
 * requests will be ignored if player is stopped. You need to at least
 * call [method@Nyx.Player.pause] before seeking and then your requested
 * seek will be handled if item could be played.
 */
void
nyx_player_seek (NyxPlayer *self, gdouble position)
{
  nyx_player_seek_custom (self, position, NYX_PLAYER_SEEK_METHOD_NORMAL);
}

/**
 * nyx_player_seek_custom:
 * @player: a #NyxPlayer
 * @position: a decimal number with position to seek to (in seconds)
 * @method: a #NyxPlayerSeekMethod
 *
 * Request the player to perform a seek operation.
 *
 * Same as [method@Nyx.Player.seek], but also allows to specify
 * [enum@Nyx.PlayerSeekMethod] to use for seek.
 */
void
nyx_player_seek_custom (NyxPlayer *self, gdouble position, NyxPlayerSeekMethod method)
{
  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (position >= 0);

  nyx_playbin_bus_post_seek (self->bus, position, method);
}

/**
 * nyx_player_advance_frame:
 * @player: a #NyxPlayer
 *
 * Request the player to perform a frame step operation.
 *
 * Note that this will pause playback automatically.
 *
 * Since: 0.10
 */
void
nyx_player_advance_frame (NyxPlayer *self)
{
  g_return_if_fail (NYX_IS_PLAYER (self));

  nyx_playbin_bus_post_advance_frame (self->bus);
}

/**
 * nyx_player_add_feature:
 * @player: a #NyxPlayer
 * @feature: a #NyxFeature
 *
 * Add another #NyxFeature to the player.
 */
void
nyx_player_add_feature (NyxPlayer *self, NyxFeature *feature)
{
  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (NYX_IS_FEATURE (feature));

  GST_OBJECT_LOCK (self);

  if (!self->features_manager)
    self->features_manager = nyx_features_manager_new ();

  GST_OBJECT_UNLOCK (self);

  /* Once a feature is added, we always have features manager object
   * and we can avoid player object locking to check that by using
   * nyx_player_get_have_features() which is atomic */
  nyx_player_set_have_features (self, TRUE);

  nyx_features_manager_add_feature (self->features_manager, feature, GST_OBJECT (self));
}

/**
 * nyx_player_make_pipeline_graph:
 * @player: a #NyxPlayer
 * @details: a #GstDebugGraphDetails level
 *
 * Make current #GStreamer pipeline graph in `graphviz` dot format.
 *
 * Applications can use tools like `graphviz` to display returned
 * data or just save it to a file as-is for the user to do it manually.
 *
 * Returns: (transfer full): current pipeline description in dot format.
 *
 * Since: 0.10
 */
gchar *
nyx_player_make_pipeline_graph (NyxPlayer *self, GstDebugGraphDetails details)
{
  g_return_val_if_fail (NYX_IS_PLAYER (self), NULL);

  return gst_debug_bin_to_dot_data (GST_BIN (self->playbin), details);
}

/**
 * nyx_player_post_message:
 * @player: a #NyxPlayer
 * @msg: (transfer full): a #GstMessage
 * @destination: a #NyxPlayerMessageDestination
 *
 * Allows sending custom messages to the desired @destination.
 *
 * This functionality can be used for communication with enhancers implementing
 * [iface@Nyx.Reactable] interface. Useful for applications to send custom messages
 * to enhacers that can react to them and/or for enhancers development to send events
 * from them to the applications. It can also be used for sending specific messages
 * from application or enhancers to the player itself.
 *
 * Messages send to the application can be received by connecting a
 * [signal@Nyx.Player::message] signal handler. Inspection of message source
 * object can be done to determine who send given message.
 *
 * Since: 0.10
 */
void
nyx_player_post_message (NyxPlayer *self, GstMessage *msg,
    NyxPlayerMessageDestination destination)
{
  g_return_if_fail (NYX_IS_PLAYER (self));
  g_return_if_fail (GST_IS_MESSAGE (msg));

  switch (destination) {
    case NYX_PLAYER_MESSAGE_DESTINATION_PLAYER:
      nyx_playbin_bus_post_user_message (self->bus, msg);
      return; // Message taken
    case NYX_PLAYER_MESSAGE_DESTINATION_REACTABLES:
      if (self->reactables_manager) {
        nyx_reactables_manager_post_message (self->reactables_manager, msg);
        return; // Message taken
      }
      break;
    case NYX_PLAYER_MESSAGE_DESTINATION_APPLICATION:
      nyx_app_bus_post_message_signal (self->app_bus,
          GST_OBJECT_CAST (self), signals[SIGNAL_MESSAGE], msg);
      break; // Above function does not take message (unref needed)
    default:
      g_assert_not_reached ();
      break;
  }

  /* Unref when not taken */
  gst_message_unref (msg);
}

static void
nyx_player_thread_start (NyxThreadedObject *threaded_object)
{
  NyxPlayer *self = NYX_PLAYER_CAST (threaded_object);
  const gchar *env, *playbin_str;
  gint i;

  GST_TRACE_OBJECT (threaded_object, "Player thread start");

  if (!(env = g_getenv ("USE_PLAYBIN3"))) // global GStreamer override
    if (!(env = g_getenv ("NYX_USE_PLAYBIN3"))) // Nyx override
      env = g_getenv ("GST_NYX_USE_PLAYBIN3"); // compat

  self->use_playbin3 = (!env || g_str_has_prefix (env, "1"));
  playbin_str = (self->use_playbin3) ? "playbin3" : "playbin";

  if (!(self->playbin = gst_element_factory_make (playbin_str, NULL))) {
    g_error ("Nyx: \"%s\" element not found, please check your setup", playbin_str);
    g_assert_not_reached ();

    return;
  }
  gst_object_ref_sink (self->playbin);

  for (i = 0; playbin_watchlist[i]; ++i)
    gst_element_add_property_notify_watch (self->playbin, playbin_watchlist[i], TRUE);

  g_signal_connect (self->playbin, "element-setup", G_CALLBACK (_element_setup_cb), self);
  g_signal_connect (self->playbin, "about-to-finish", G_CALLBACK (_about_to_finish_cb), self);

  if (!self->use_playbin3) {
    g_signal_connect (self->playbin, "video-changed", G_CALLBACK (_playbin_streams_changed_cb), self);
    g_signal_connect (self->playbin, "audio-changed", G_CALLBACK (_playbin_streams_changed_cb), self);
    g_signal_connect (self->playbin, "text-changed", G_CALLBACK (_playbin_streams_changed_cb), self);

    g_signal_connect (self->playbin, "video-tags-changed", G_CALLBACK (_playbin_video_tags_changed_cb), self);
    g_signal_connect (self->playbin, "audio-tags-changed", G_CALLBACK (_playbin_audio_tags_changed_cb), self);
    g_signal_connect (self->playbin, "text-tags-changed", G_CALLBACK (_playbin_text_tags_changed_cb), self);

    g_signal_connect (self->playbin, "notify::current-video", G_CALLBACK (_playbin_selected_streams_changed_cb), self);
    g_signal_connect (self->playbin, "notify::current-audio", G_CALLBACK (_playbin_selected_streams_changed_cb), self);
    g_signal_connect (self->playbin, "notify::current-text", G_CALLBACK (_playbin_selected_streams_changed_cb), self);
  }

  self->bus = gst_element_get_bus (self->playbin);
  gst_bus_add_watch (self->bus, (GstBusFunc) nyx_playbin_bus_message_func, self);
}

static void
nyx_player_thread_stop (NyxThreadedObject *threaded_object)
{
  NyxPlayer *self = NYX_PLAYER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Player thread stop");

  nyx_player_remove_tick_source (self);

  gst_bus_set_flushing (self->bus, TRUE);
  gst_bus_remove_watch (self->bus);

  gst_bus_set_flushing (GST_BUS_CAST (self->app_bus), TRUE);
  gst_bus_remove_watch (GST_BUS_CAST (self->app_bus));

  nyx_player_reset (self, TRUE);

  gst_element_set_state (self->playbin, GST_STATE_NULL);

  gst_clear_object (&self->bus);
  gst_clear_object (&self->app_bus);
  gst_clear_object (&self->playbin);
  gst_clear_object (&self->collection);
}

static void
nyx_player_init (NyxPlayer *self)
{
  self->queue = nyx_queue_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->queue), GST_OBJECT_CAST (self));

  self->video_streams = nyx_stream_list_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->video_streams), GST_OBJECT_CAST (self));

  self->audio_streams = nyx_stream_list_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->audio_streams), GST_OBJECT_CAST (self));

  self->subtitle_streams = nyx_stream_list_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->subtitle_streams), GST_OBJECT_CAST (self));

  self->enhancer_proxies = nyx_enhancer_proxy_list_new_named (NULL);
  gst_object_set_parent (GST_OBJECT_CAST (self->enhancer_proxies), GST_OBJECT_CAST (self));

  nyx_enhancer_proxy_list_fill_from_global_proxies (self->enhancer_proxies);

  if (nyx_enhancer_proxy_list_has_proxy_with_interface (self->enhancer_proxies, NYX_TYPE_REACTABLE)) {
    self->reactables_manager = nyx_reactables_manager_new ();
    gst_object_set_parent (GST_OBJECT_CAST (self->reactables_manager), GST_OBJECT_CAST (self));
  }

  self->position_query = gst_query_new_position (GST_FORMAT_TIME);

  self->current_state = GST_STATE_NULL;
  self->target_state = GST_STATE_READY;

  self->autoplay = DEFAULT_AUTOPLAY;
  self->mute = DEFAULT_MUTE;
  self->volume = DEFAULT_VOLUME;
  self->speed = DEFAULT_SPEED;
  self->state = DEFAULT_STATE;
  self->video_enabled = DEFAULT_VIDEO_ENABLED;
  self->audio_enabled = DEFAULT_AUDIO_ENABLED;
  self->subtitles_enabled = DEFAULT_SUBTITLES_ENABLED;
  self->download_enabled = DEFAULT_DOWNLOAD_ENABLED;
  self->start_bitrate = DEFAULT_ADAPTIVE_START_BITRATE;
}

static void
nyx_player_constructed (GObject *object)
{
  NyxPlayer *self = NYX_PLAYER_CAST (object);

  GST_OBJECT_LOCK (self);
  self->app_bus = nyx_app_bus_new ();
  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
nyx_player_dispose (GObject *object)
{
  NyxPlayer *self = NYX_PLAYER_CAST (object);

  GST_OBJECT_LOCK (self);

  if (self->stream_notify_id != 0) {
    g_signal_handler_disconnect (self->collection, self->stream_notify_id);
    self->stream_notify_id = 0;
  }

  GST_OBJECT_UNLOCK (self);

  /* Parent class will wait for player thread to stop running */
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_player_finalize (GObject *object)
{
  NyxPlayer *self = NYX_PLAYER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_object_unparent (GST_OBJECT_CAST (self->queue));
  gst_object_unref (self->queue);

  gst_object_unparent (GST_OBJECT_CAST (self->video_streams));
  gst_object_unref (self->video_streams);

  gst_object_unparent (GST_OBJECT_CAST (self->audio_streams));
  gst_object_unref (self->audio_streams);

  gst_object_unparent (GST_OBJECT_CAST (self->subtitle_streams));
  gst_object_unref (self->subtitle_streams);

  if (self->reactables_manager) {
    gst_object_unparent (GST_OBJECT_CAST (self->reactables_manager));
    gst_object_unref (self->reactables_manager);
  }

  gst_object_unparent (GST_OBJECT_CAST (self->enhancer_proxies));
  gst_object_unref (self->enhancer_proxies);

  gst_query_unref (self->position_query);

  gst_clear_object (&self->collection);
  gst_clear_object (&self->features_manager);
  gst_clear_object (&self->pending_item);
  gst_clear_object (&self->played_item);

  g_free (self->download_dir);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_player_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxPlayer *self = NYX_PLAYER_CAST (object);

  switch (prop_id) {
    case PROP_QUEUE:
      g_value_set_object (value, nyx_player_get_queue (self));
      break;
    case PROP_VIDEO_STREAMS:
      g_value_set_object (value, nyx_player_get_video_streams (self));
      break;
    case PROP_AUDIO_STREAMS:
      g_value_set_object (value, nyx_player_get_audio_streams (self));
      break;
    case PROP_SUBTITLE_STREAMS:
      g_value_set_object (value, nyx_player_get_subtitle_streams (self));
      break;
    case PROP_ENHANCER_PROXIES:
      g_value_set_object (value, nyx_player_get_enhancer_proxies (self));
      break;
    case PROP_AUTOPLAY:
      g_value_set_boolean (value, nyx_player_get_autoplay (self));
      break;
    case PROP_POSITION:
      g_value_set_double (value, nyx_player_get_position (self));
      break;
    case PROP_SPEED:
      g_value_set_double (value, nyx_player_get_speed (self));
      break;
    case PROP_STATE:
      g_value_set_enum (value, nyx_player_get_state (self));
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, nyx_player_get_mute (self));
      break;
    case PROP_VOLUME:
      g_value_set_double (value, nyx_player_get_volume (self));
      break;
    case PROP_AUDIO_SINK:
      g_value_take_object (value, nyx_player_get_audio_sink (self));
      break;
    case PROP_VIDEO_SINK:
      g_value_take_object (value, nyx_player_get_video_sink (self));
      break;
    case PROP_AUDIO_FILTER:
      g_value_take_object (value, nyx_player_get_audio_filter (self));
      break;
    case PROP_VIDEO_FILTER:
      g_value_take_object (value, nyx_player_get_video_filter (self));
      break;
    case PROP_CURRENT_AUDIO_DECODER:
      g_value_take_object (value, nyx_player_get_current_audio_decoder (self));
      break;
    case PROP_CURRENT_VIDEO_DECODER:
      g_value_take_object (value, nyx_player_get_current_video_decoder (self));
      break;
    case PROP_VIDEO_ENABLED:
      g_value_set_boolean (value, nyx_player_get_video_enabled (self));
      break;
    case PROP_AUDIO_ENABLED:
      g_value_set_boolean (value, nyx_player_get_audio_enabled (self));
      break;
    case PROP_SUBTITLES_ENABLED:
      g_value_set_boolean (value, nyx_player_get_subtitles_enabled (self));
      break;
    case PROP_DOWNLOAD_DIR:
      g_value_take_string (value, nyx_player_get_download_dir (self));
      break;
    case PROP_DOWNLOAD_ENABLED:
      g_value_set_boolean (value, nyx_player_get_download_enabled (self));
      break;
    case PROP_ADAPTIVE_START_BITRATE:
      g_value_set_uint (value, nyx_player_get_adaptive_start_bitrate (self));
      break;
    case PROP_ADAPTIVE_MIN_BITRATE:
      g_value_set_uint (value, nyx_player_get_adaptive_min_bitrate (self));
      break;
    case PROP_ADAPTIVE_MAX_BITRATE:
      g_value_set_uint (value, nyx_player_get_adaptive_max_bitrate (self));
      break;
    case PROP_ADAPTIVE_BANDWIDTH:
      g_value_set_uint (value, nyx_player_get_adaptive_bandwidth (self));
      break;
    case PROP_AUDIO_OFFSET:
      g_value_set_double (value, nyx_player_get_audio_offset (self));
      break;
    case PROP_SUBTITLE_OFFSET:
      g_value_set_double (value, nyx_player_get_subtitle_offset (self));
      break;
    case PROP_SUBTITLE_FONT_DESC:
      g_value_take_string (value, nyx_player_get_subtitle_font_desc (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_player_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxPlayer *self = NYX_PLAYER_CAST (object);

  switch (prop_id) {
    case PROP_AUTOPLAY:
      nyx_player_set_autoplay (self, g_value_get_boolean (value));
      break;
    case PROP_SPEED:
      nyx_player_set_speed (self, g_value_get_double (value));
      break;
    case PROP_MUTE:
      nyx_player_set_mute (self, g_value_get_boolean (value));
      break;
    case PROP_VOLUME:
      nyx_player_set_volume (self, g_value_get_double (value));
      break;
    case PROP_AUDIO_SINK:
      nyx_player_set_audio_sink (self, g_value_get_object (value));
      break;
    case PROP_VIDEO_SINK:
      nyx_player_set_video_sink (self, g_value_get_object (value));
      break;
    case PROP_AUDIO_FILTER:
      nyx_player_set_audio_filter (self, g_value_get_object (value));
      break;
    case PROP_VIDEO_FILTER:
      nyx_player_set_video_filter (self, g_value_get_object (value));
      break;
    case PROP_VIDEO_ENABLED:
      nyx_player_set_video_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_AUDIO_ENABLED:
      nyx_player_set_audio_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_SUBTITLES_ENABLED:
      nyx_player_set_subtitles_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_DOWNLOAD_DIR:
      nyx_player_set_download_dir (self, g_value_get_string (value));
      break;
    case PROP_DOWNLOAD_ENABLED:
      nyx_player_set_download_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_ADAPTIVE_START_BITRATE:
      nyx_player_set_adaptive_start_bitrate (self, g_value_get_uint (value));
      break;
    case PROP_ADAPTIVE_MIN_BITRATE:
      nyx_player_set_adaptive_min_bitrate (self, g_value_get_uint (value));
      break;
    case PROP_ADAPTIVE_MAX_BITRATE:
      nyx_player_set_adaptive_max_bitrate (self, g_value_get_uint (value));
      break;
    case PROP_AUDIO_OFFSET:
      nyx_player_set_audio_offset (self, g_value_get_double (value));
      break;
    case PROP_SUBTITLE_OFFSET:
      nyx_player_set_subtitle_offset (self, g_value_get_double (value));
      break;
    case PROP_SUBTITLE_FONT_DESC:
      nyx_player_set_subtitle_font_desc (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_player_class_init (NyxPlayerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxThreadedObjectClass *threaded_object = (NyxThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxplayer", 0,
      "Nyx Player");

  gobject_class->constructed = nyx_player_constructed;
  gobject_class->get_property = nyx_player_get_property;
  gobject_class->set_property = nyx_player_set_property;
  gobject_class->dispose = nyx_player_dispose;
  gobject_class->finalize = nyx_player_finalize;

  /**
   * NyxPlayer:queue:
   *
   * Nyx playback queue.
   */
  param_specs[PROP_QUEUE] = g_param_spec_object ("queue",
      NULL, NULL, NYX_TYPE_QUEUE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:video-streams:
   *
   * List of currently available video streams.
   */
  param_specs[PROP_VIDEO_STREAMS] = g_param_spec_object ("video-streams",
      NULL, NULL, NYX_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:audio-streams:
   *
   * List of currently available audio streams.
   */
  param_specs[PROP_AUDIO_STREAMS] = g_param_spec_object ("audio-streams",
      NULL, NULL, NYX_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:subtitle-streams:
   *
   * List of currently available subtitle streams.
   */
  param_specs[PROP_SUBTITLE_STREAMS] = g_param_spec_object ("subtitle-streams",
      NULL, NULL, NYX_TYPE_STREAM_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:enhancer-proxies:
   *
   * List of available enhancers in the form of [class@Nyx.EnhancerProxy] objects.
   *
   * Use these to inspect available enhancers on the system and configure
   * their properties on a per player instance basis.
   *
   * Since: 0.10
   */
  param_specs[PROP_ENHANCER_PROXIES] = g_param_spec_object ("enhancer-proxies",
      NULL, NULL, NYX_TYPE_ENHANCER_PROXY_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:autoplay:
   *
   * Always try to start playback after media item changes.
   */
  param_specs[PROP_AUTOPLAY] = g_param_spec_boolean ("autoplay",
      NULL, NULL, DEFAULT_AUTOPLAY,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:position:
   *
   * Current playback position as a decimal number in seconds.
   */
  param_specs[PROP_POSITION] = g_param_spec_double ("position",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:speed:
   *
   * Current playback speed.
   */
  param_specs[PROP_SPEED] = g_param_spec_double ("speed",
      NULL, NULL, G_MINDOUBLE, G_MAXDOUBLE, DEFAULT_SPEED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:state:
   *
   * Current playback state.
   */
  param_specs[PROP_STATE] = g_param_spec_enum ("state",
      NULL, NULL, NYX_TYPE_PLAYER_STATE, DEFAULT_STATE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:mute:
   *
   * Mute audio without changing volume.
   */
  param_specs[PROP_MUTE] = g_param_spec_boolean ("mute",
      NULL, NULL, DEFAULT_MUTE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:volume:
   *
   * Current volume as a decimal number (1.0 = 100%).
   *
   * Note that #NyxPlayer uses a CUBIC volume scale, meaning
   * that this property value reflects human hearing level and can
   * be easily bound to volume sliders as-is.
   */
  param_specs[PROP_VOLUME] = g_param_spec_double ("volume",
      NULL, NULL, 0, 2.0, DEFAULT_VOLUME,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:video-sink:
   *
   * Video sink to use (autovideosink by default).
   */
  param_specs[PROP_VIDEO_SINK] = g_param_spec_object ("video-sink",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:audio-sink:
   *
   * Audio sink to use (autoaudiosink by default).
   */
  param_specs[PROP_AUDIO_SINK] = g_param_spec_object ("audio-sink",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:video-filter:
   *
   * Optional video filter to use (none by default).
   */
  param_specs[PROP_VIDEO_FILTER] = g_param_spec_object ("video-filter",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:audio-filter:
   *
   * Optional audio filter to use (none by default).
   */
  param_specs[PROP_AUDIO_FILTER] = g_param_spec_object ("audio-filter",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:current-video-decoder:
   *
   * Currently used video decoder.
   */
  param_specs[PROP_CURRENT_VIDEO_DECODER] = g_param_spec_object ("current-video-decoder",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:current-audio-decoder:
   *
   * Currently used audio decoder.
   */
  param_specs[PROP_CURRENT_AUDIO_DECODER] = g_param_spec_object ("current-audio-decoder",
      NULL, NULL, GST_TYPE_ELEMENT,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:video-enabled:
   *
   * Whether video stream is enabled.
   */
  param_specs[PROP_VIDEO_ENABLED] = g_param_spec_boolean ("video-enabled",
      NULL, NULL, DEFAULT_VIDEO_ENABLED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:audio-enabled:
   *
   * Whether audio stream is enabled.
   */
  param_specs[PROP_AUDIO_ENABLED] = g_param_spec_boolean ("audio-enabled",
      NULL, NULL, DEFAULT_AUDIO_ENABLED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:subtitles-enabled:
   *
   * Whether subtitles stream is enabled.
   */
  param_specs[PROP_SUBTITLES_ENABLED] = g_param_spec_boolean ("subtitles-enabled",
      NULL, NULL, DEFAULT_SUBTITLES_ENABLED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:download-dir:
   *
   * A directory that @player will use to download network content
   * when [property@Nyx.Player:download-enabled] is set to %TRUE.
   *
   * If directory at @path does not exist, it will be automatically created.
   *
   * Since: 0.8
   */
  param_specs[PROP_DOWNLOAD_DIR] = g_param_spec_string ("download-dir",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:download-enabled:
   *
   * Whether progressive download buffering is enabled.
   *
   * If progressive download is enabled and [property@Nyx.Player:download-dir]
   * is set, streamed network content will be cached to the disk space instead
   * of memory whenever possible. This allows for faster seeking through
   * currently played media.
   *
   * Not every type of content is download applicable. Mainly applies to
   * web content that does not use adaptive streaming.
   *
   * Once data that media item URI points to is fully downloaded, player
   * will emit [signal@Nyx.Player::download-complete] signal with a
   * location of downloaded file.
   *
   * Playing again the exact same [class@Nyx.MediaItem] object that was
   * previously fully downloaded will cause player to automatically use that
   * cached file if it still exists, avoiding any further network requests.
   *
   * Please note that player will not delete nor manage downloaded content.
   * It is up to application to cleanup data in created cache directory
   * (e.g. before app exits), in order to remove any downloads that app
   * is not going to use next time it is run and incomplete ones.
   *
   * Since: 0.8
   */
  param_specs[PROP_DOWNLOAD_ENABLED] = g_param_spec_boolean ("download-enabled",
      NULL, NULL, DEFAULT_DOWNLOAD_ENABLED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:adaptive-start-bitrate:
   *
   * An initial bitrate (bits/s) to select during
   * starting adaptive streaming such as DASH or HLS.
   *
   * If value is lower than the lowest available bitrate in streaming
   * manifest, then lowest possible bitrate will be selected.
   *
   * Since: 0.8
   */
  param_specs[PROP_ADAPTIVE_START_BITRATE] = g_param_spec_uint ("adaptive-start-bitrate",
      NULL, NULL, 0, G_MAXUINT, DEFAULT_ADAPTIVE_START_BITRATE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:adaptive-min-bitrate:
   *
   * A minimal allowed bitrate (bits/s) during adaptive streaming
   * such as DASH or HLS.
   *
   * Setting this will prevent streaming from entering lower qualities
   * (even when connection speed cannot keep up). When set together with
   * [property@Nyx.Player:adaptive-max-bitrate] it can be used to
   * enforce some specific quality.
   *
   * Since: 0.8
   */
  param_specs[PROP_ADAPTIVE_MIN_BITRATE] = g_param_spec_uint ("adaptive-min-bitrate",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:adaptive-max-bitrate:
   *
   * A maximal allowed bitrate (bits/s) during adaptive streaming
   * such as DASH or HLS (`0` for unlimited).
   *
   * Setting this will prevent streaming from entering qualities with
   * higher bandwidth than the one set. When set together with
   * [property@Nyx.Player:adaptive-min-bitrate] it can be used to
   * enforce some specific quality.
   *
   * Since: 0.8
   */
  param_specs[PROP_ADAPTIVE_MAX_BITRATE] = g_param_spec_uint ("adaptive-max-bitrate",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:adaptive-bandwidth:
   *
   * Last fragment download bandwidth (bits/s) during adaptive streaming.
   *
   * This property only changes when adaptive streaming and later stays
   * at the last value until streaming some adaptive content again.
   *
   * Apps can use this to determine and set an optimal value for
   * [property@Nyx.Player:adaptive-start-bitrate].
   *
   * Since: 0.8
   */
  param_specs[PROP_ADAPTIVE_BANDWIDTH] = g_param_spec_uint ("adaptive-bandwidth",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:audio-offset:
   *
   * Audio stream offset relative to video.
   */
  param_specs[PROP_AUDIO_OFFSET] = g_param_spec_double ("audio-offset",
      NULL, NULL, G_MININT64, G_MAXINT64, 0, // NOTE: Gstreamer has gint64 range
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:subtitle-offset:
   *
   * Subtitle stream offset relative to video.
   */
  param_specs[PROP_SUBTITLE_OFFSET] = g_param_spec_double ("subtitle-offset",
      NULL, NULL, G_MININT64, G_MAXINT64, 0, // NOTE: Gstreamer has gint64 range
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer:subtitle-font-desc:
   *
   * Subtitle stream font description.
   */
  param_specs[PROP_SUBTITLE_FONT_DESC] = g_param_spec_string ("subtitle-font-desc",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxPlayer::seek-done:
   * @player: a #NyxPlayer
   *
   * A seeking operation has finished. Player is now at playback position after seek.
   */
  signals[SIGNAL_SEEK_DONE] = g_signal_new ("seek-done",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * NyxPlayer::download-complete:
   * @player: a #NyxPlayer
   * @item: a #NyxMediaItem
   * @location: (type filename): a path to downloaded file
   *
   * Media was fully downloaded to local cache directory. This signal will
   * be only emitted when progressive download buffering is enabled by
   * setting [property@Nyx.Player:download-enabled] property to %TRUE.
   *
   * Download cache file location can also be read directly from @item
   * through its [property@Nyx.MediaItem:cache-location] property.
   *
   * Since: 0.8
   */
  signals[SIGNAL_DOWNLOAD_COMPLETE] = g_signal_new ("download-complete",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, NYX_TYPE_MEDIA_ITEM, G_TYPE_STRING);

  /**
   * NyxPlayer::missing-plugin:
   * @player: a #NyxPlayer
   * @name: a localised string describing the missing feature, for use in
   *   error dialogs and the like.
   * @installer_detail: (nullable): a string containing all the details about the missing
   *   element to be passed to an external installer called via either
   *   gst_install_plugins_async() or gst_install_plugins_sync() function.
   *
   * A #GStreamer plugin or one of its features needed for playback is missing.
   *
   * The @description and @installer_detail can be used to present the user more info
   * about what is missing and prompt him to install it with an external installer.
   */
  signals[SIGNAL_MISSING_PLUGIN] = g_signal_new ("missing-plugin",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * NyxPlayer::message:
   * @player: a #NyxPlayer
   * @msg: a #GstMessage
   *
   * Allows for applications to receive element messages posted
   * on the underlaying pipeline bus.
   *
   * This is a detailed signal. Connect to it via `message::name`
   * to only receive messages with a certain `name`.
   *
   * Player will only forward messages to the main app thread (from which
   * this signal is emitted) that have a matching signal handler, thus
   * it is more efficient to listen only for specific messages instead
   * of connecting to simply `message` with no details (without message name).
   *
   * Since: 0.10
   */
  signals[SIGNAL_MESSAGE] = g_signal_new ("message", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS | G_SIGNAL_DETAILED,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_MESSAGE);

  /**
   * NyxPlayer::warning:
   * @player: a #NyxPlayer
   * @error: a #GError
   * @debug_info: (nullable): an additional debug message.
   *
   * These are some usually more minor error messages that should
   * be treated like warnings. Should not generally prevent/stop playback.
   */
  signals[SIGNAL_WARNING] = g_signal_new ("warning",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, G_TYPE_STRING);

  /**
   * NyxPlayer::error:
   * @player: a #NyxPlayer
   * @error: a #GError
   * @debug_info: (nullable): an additional debug message.
   *
   * These are normal error messages. Upon emitting this signal,
   * playback will stop due to the error.
   */
  signals[SIGNAL_ERROR] = g_signal_new ("error",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_ERROR, G_TYPE_STRING);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  threaded_object->thread_start = nyx_player_thread_start;
  threaded_object->thread_stop = nyx_player_thread_stop;
}
