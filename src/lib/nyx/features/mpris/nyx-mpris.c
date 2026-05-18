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

/**
 * NyxMpris:
 *
 * An optional `MPRIS` feature to add to the player.
 *
 * Not every OS supports `MPRIS`. Use [const@Nyx.HAVE_MPRIS] macro
 * to check if Nyx API was compiled with this feature.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */

#include "nyx-mpris.h"
#include "nyx-mpris-gdbus.h"
#include "nyx-player.h"
#include "nyx-utils-private.h"

#define NYX_MPRIS_DO_WITH_PLAYER(mpris, _player_dst, ...) {                           \
    *_player_dst = NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (mpris))); \
    if (G_LIKELY (*_player_dst != NULL))                                                  \
      __VA_ARGS__                                                                         \
    gst_clear_object (_player_dst); }

#define NYX_MPRIS_SECONDS_TO_USECONDS(seconds) ((gint64) (seconds * G_GINT64_CONSTANT (1000000)))
#define NYX_MPRIS_USECONDS_TO_SECONDS(useconds) ((gdouble) useconds / G_GINT64_CONSTANT (1000000))

#define NYX_MPRIS_COMPARE(a,b) (strcmp (a,b) == 0)

#define NYX_MPRIS_NO_TRACK "/org/mpris/MediaPlayer2/TrackList/NoTrack"

#define NYX_MPRIS_PLAYBACK_STATUS_PLAYING "Playing"
#define NYX_MPRIS_PLAYBACK_STATUS_PAUSED "Paused"
#define NYX_MPRIS_PLAYBACK_STATUS_STOPPED "Stopped"

#define NYX_MPRIS_LOOP_NONE "None"
#define NYX_MPRIS_LOOP_TRACK "Track"
#define NYX_MPRIS_LOOP_PLAYLIST "Playlist"

#define DEFAULT_QUEUE_CONTROLLABLE FALSE

#define GST_CAT_DEFAULT nyx_mpris_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct
{
  gchar *id;
  NyxMediaItem *item;
} NyxMprisTrack;

struct _NyxMpris
{
  NyxFeature parent;

  NyxMprisMediaPlayer2 *base_skeleton;
  NyxMprisMediaPlayer2Player *player_skeleton;
  NyxMprisMediaPlayer2TrackList *tracks_skeleton;

  gboolean base_exported;
  gboolean player_exported;
  gboolean tracks_exported;

  guint name_id;
  gboolean registered;

  GMainLoop *loop;

  GPtrArray *tracks;
  NyxMprisTrack *current_track;

  NyxQueueProgressionMode default_mode;
  NyxQueueProgressionMode non_shuffle_mode;

  gchar *own_name;
  gchar *identity;
  gchar *desktop_entry;

  gint queue_controllable; // atomic
  gchar *fallback_art_url;
};

enum
{
  PROP_0,
  PROP_OWN_NAME,
  PROP_IDENTITY,
  PROP_DESKTOP_ENTRY,
  PROP_QUEUE_CONTROLLABLE,
  PROP_FALLBACK_ART_URL,
  PROP_LAST
};

#define parent_class nyx_mpris_parent_class
G_DEFINE_TYPE (NyxMpris, nyx_mpris, NYX_TYPE_FEATURE);

static const gchar *const empty_tracklist[] = { NULL, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static NyxMprisTrack *
nyx_mpris_track_new (NyxMediaItem *item)
{
  NyxMprisTrack *track = g_new (NyxMprisTrack, 1);

  /* MPRIS docs: "Media players may not use any paths starting with /org/mpris
   * unless explicitly allowed by this specification. Such paths are intended to
   * have special meaning, such as /org/mpris/MediaPlayer2/TrackList/NoTrack" */
  track->id = g_strdup_printf ("/org/nyx/MediaItem%u",
      nyx_media_item_get_id (item));

  track->item = gst_object_ref (item);

  GST_TRACE ("Created track: %s", track->id);

  return track;
}

static void
nyx_mpris_track_free (NyxMprisTrack *track)
{
  GST_TRACE ("Freeing track: %s", track->id);

  g_free (track->id);
  gst_object_unref (track->item);

  g_free (track);
}

static inline void
_mpris_read_initial_tracks (NyxMpris *self, NyxQueue *queue)
{
  NyxMediaItem *item, *current_item;
  guint i = 0;

  current_item = nyx_queue_get_current_item (queue);

  while ((item = nyx_queue_get_item (queue, i))) {
    NyxMprisTrack *track = nyx_mpris_track_new (item);

    if (track->item == current_item)
      self->current_track = track;

    g_ptr_array_add (self->tracks, track);

    gst_object_unref (item);
    i++;
  }

  gst_clear_object (&current_item);
}

static gboolean
_mpris_find_track_by_item (NyxMpris *self, NyxMediaItem *search_item, guint *index)
{
  guint i;

  for (i = 0; i < self->tracks->len; ++i) {
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, i);

    if (search_item == track->item) {
      if (index)
        *index = i;

      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
_mpris_find_track_by_id (NyxMpris *self, const gchar *search_id, guint *index)
{
  guint i;

  for (i = 0; i < self->tracks->len; ++i) {
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, i);

    if (NYX_MPRIS_COMPARE (track->id, search_id)) {
      if (index)
        *index = i;

      return TRUE;
    }
  }

  return FALSE;
}

static GVariant *
_mpris_build_track_metadata (NyxMpris *self, NyxMprisTrack *track)
{
  GVariantBuilder builder;
  GVariant *variant;
  const gchar *uri;
  gchar *title;
  gint64 duration;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  uri = nyx_media_item_get_uri (track->item);
  title = nyx_media_item_get_title (track->item);
  duration = NYX_MPRIS_SECONDS_TO_USECONDS (
      nyx_media_item_get_duration (track->item));

  g_variant_builder_add (&builder, "{sv}", "mpris:trackid",
      g_variant_new_string (track->id));
  g_variant_builder_add (&builder, "{sv}", "mpris:length",
      g_variant_new_int64 (duration));
  g_variant_builder_add (&builder, "{sv}", "xesam:url",
      g_variant_new_string (uri));
  if (title) {
    g_variant_builder_add (&builder, "{sv}", "xesam:title",
        g_variant_new_string (title));
  }

  /* TODO: Fill more xesam props from tags within media info */

  GST_OBJECT_LOCK (self);

  /* TODO: Support image sample or per-item custom artwork */
  if (self->fallback_art_url) {
    g_variant_builder_add (&builder, "{sv}", "mpris:artUrl",
        g_variant_new_string (self->fallback_art_url));
  }

  GST_OBJECT_UNLOCK (self);

  variant = g_variant_builder_end (&builder);

  g_free (title);

  return variant;
}

static gchar **
_filter_names (const gchar *const *all_names)
{
  GStrvBuilder *builder;
  gchar **filtered_names;
  guint i;

  builder = g_strv_builder_new ();

  for (i = 0; all_names[i]; ++i) {
    const gchar *const *remaining_names = all_names + i + 1;

    if (*remaining_names && g_strv_contains (remaining_names, all_names[i]))
      continue;

    GST_LOG ("Found: %s", all_names[i]);
    g_strv_builder_add (builder, all_names[i]);
  }

  filtered_names = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  return filtered_names;
}

static gchar **
nyx_mpris_get_supported_uri_schemes (NyxMpris *self)
{
  GStrvBuilder *builder;
  gchar **all_schemes, **filtered_schemes;
  GList *elements, *el;
  guint i;

  GST_DEBUG_OBJECT (self, "Checking supported URI schemes");

  builder = g_strv_builder_new ();
  elements = gst_element_factory_list_get_elements (
      GST_ELEMENT_FACTORY_TYPE_SRC, GST_RANK_NONE);

  for (el = elements; el != NULL; el = el->next) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (el->data);
    const gchar *const *protocols;

    if (gst_element_factory_get_uri_type (factory) != GST_URI_SRC)
      continue;

    if (!(protocols = gst_element_factory_get_uri_protocols (factory)))
      continue;

    for (i = 0; protocols[i]; ++i)
      g_strv_builder_add (builder, protocols[i]);
  }

  all_schemes = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  gst_plugin_feature_list_free (elements);

  filtered_schemes = _filter_names ((const gchar *const *) all_schemes);
  g_strfreev (all_schemes);

  return filtered_schemes;
}

static gchar **
nyx_mpris_get_supported_mime_types (NyxMpris *self)
{
  GStrvBuilder *builder;
  gchar **all_types, **filtered_types;
  GList *elements, *el;

  GST_DEBUG_OBJECT (self, "Checking supported mime-types");

  builder = g_strv_builder_new ();
  elements = gst_element_factory_list_get_elements (
      GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_NONE);

  for (el = elements; el != NULL; el = el->next) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (el->data);
    const GList *pad_templates, *pt;

    pad_templates = gst_element_factory_get_static_pad_templates (factory);

    for (pt = pad_templates; pt != NULL; pt = pt->next) {
      GstStaticPadTemplate *template = (GstStaticPadTemplate *) pt->data;
      GstCaps *caps;
      guint i, size;

      if (template->direction != GST_PAD_SINK)
        continue;

      caps = gst_static_pad_template_get_caps (template);
      size = gst_caps_get_size (caps);

      for (i = 0; i < size; ++i) {
        GstStructure *structure = gst_caps_get_structure (caps, i);
        const gchar *name = gst_structure_get_name (structure);

        /* Skip GStreamer internal mime types */
        if (g_str_has_prefix (name, "application/x-gst-"))
          continue;

        /* GStreamer uses "video/quicktime" for MP4. If we can
         * handle it, then also add more generic ones. */
        if (strcmp (name, "video/quicktime") == 0) {
          g_strv_builder_add (builder, "video/mp4");
          g_strv_builder_add (builder, "audio/mp4");
        }

        g_strv_builder_add (builder, name);
      }

      gst_caps_unref (caps);
    }
  }

  all_types = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  gst_plugin_feature_list_free (elements);

  filtered_types = _filter_names ((const gchar *const *) all_types);
  g_strfreev (all_types);

  return filtered_types;
}

static void
nyx_mpris_unregister (NyxMpris *self)
{
  GST_DEBUG_OBJECT (self, "Unregister");

  if (self->base_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->base_skeleton));
    self->base_exported = FALSE;
  }
  if (self->player_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->player_skeleton));
    self->player_exported = FALSE;
  }
  if (self->tracks_exported) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->tracks_skeleton));
    self->tracks_exported = FALSE;
  }

  self->registered = FALSE;
}

static void
nyx_mpris_refresh_current_track (NyxMpris *self, GVariant *variant)
{
  gboolean is_live = FALSE;

  GST_LOG_OBJECT (self, "Current track refresh");

  /* Set or clear metadata */
  nyx_mpris_media_player2_player_set_metadata (self->player_skeleton, variant);

  /* Properties related to media item availablity, not current state */
  nyx_mpris_media_player2_player_set_can_play (self->player_skeleton, self->current_track != NULL);
  nyx_mpris_media_player2_player_set_can_pause (self->player_skeleton, self->current_track != NULL);

  /* FIXME: Also disable for LIVE content */
  nyx_mpris_media_player2_player_set_can_seek (self->player_skeleton, self->current_track != NULL);
  nyx_mpris_media_player2_player_set_minimum_rate (self->player_skeleton, (is_live) ? 1.0 : G_MINDOUBLE);
  nyx_mpris_media_player2_player_set_maximum_rate (self->player_skeleton, (is_live) ? 1.0 : G_MAXDOUBLE);
}

static void
nyx_mpris_refresh_track (NyxMpris *self, NyxMprisTrack *track)
{
  GVariant *variant = g_variant_take_ref (_mpris_build_track_metadata (self, track));

  if (track == self->current_track)
    nyx_mpris_refresh_current_track (self, variant);

  nyx_mpris_media_player2_track_list_emit_track_metadata_changed (self->tracks_skeleton,
      track->id, variant);

  g_variant_unref (variant);
}

static void
nyx_mpris_refresh_all_tracks (NyxMpris *self)
{
  guint i;

  for (i = 0; i < self->tracks->len; ++i) {
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, i);
    nyx_mpris_refresh_track (self, track);
  }
}

static void
nyx_mpris_refresh_track_list (NyxMpris *self)
{
  GStrvBuilder *builder = NULL;
  gchar **tracks_ids;
  guint i;

  GST_LOG_OBJECT (self, "Track list refresh");

  /* Track list is empty */
  if (self->tracks->len == 0) {
    nyx_mpris_media_player2_track_list_set_tracks (self->tracks_skeleton, empty_tracklist);
    return;
  }

  builder = g_strv_builder_new ();

  for (i = 0; i < self->tracks->len; ++i) {
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, i);
    g_strv_builder_add (builder, track->id);
  }

  tracks_ids = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  nyx_mpris_media_player2_track_list_set_tracks (self->tracks_skeleton, (const gchar *const *) tracks_ids);
  g_strfreev (tracks_ids);
}

static void
nyx_mpris_refresh_can_go_next_previous (NyxMpris *self)
{
  gboolean can_previous = FALSE, can_next = FALSE;

  GST_LOG_OBJECT (self, "Next/Previous availability refresh");

  if (self->current_track && nyx_mpris_get_queue_controllable (self)) {
    guint index = 0;

    if (_mpris_find_track_by_item (self, self->current_track->item, &index)) {
      can_previous = (index > 0);
      can_next = (index < self->tracks->len - 1);
    }
  }

  nyx_mpris_media_player2_player_set_can_go_previous (self->player_skeleton, can_previous);
  nyx_mpris_media_player2_player_set_can_go_next (self->player_skeleton, can_next);
}

static void
nyx_mpris_state_changed (NyxFeature *feature, NyxPlayerState state)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  const gchar *status_str = NYX_MPRIS_PLAYBACK_STATUS_STOPPED;

  switch (state) {
    case NYX_PLAYER_STATE_PLAYING:
      status_str = NYX_MPRIS_PLAYBACK_STATUS_PLAYING;
      break;
    case NYX_PLAYER_STATE_PAUSED:
    case NYX_PLAYER_STATE_BUFFERING:
      status_str = NYX_MPRIS_PLAYBACK_STATUS_PAUSED;
      break;
    default:
      break;
  }

  GST_DEBUG_OBJECT (self, "Playback status changed to: %s", status_str);
  nyx_mpris_media_player2_player_set_playback_status (self->player_skeleton, status_str);
}

static void
nyx_mpris_position_changed (NyxFeature *feature, gdouble position)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);

  GST_LOG_OBJECT (self, "Position changed to: %lf", position);
  nyx_mpris_media_player2_player_set_position (self->player_skeleton,
      NYX_MPRIS_SECONDS_TO_USECONDS (position));
}

static void
nyx_mpris_speed_changed (NyxFeature *feature, gdouble speed)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  gdouble mpris_speed;

  mpris_speed = nyx_mpris_media_player2_player_get_rate (self->player_skeleton);

  if (!G_APPROX_VALUE (speed, mpris_speed, FLT_EPSILON)) {
    GST_LOG_OBJECT (self, "Speed changed to: %lf", speed);
    nyx_mpris_media_player2_player_set_rate (self->player_skeleton, speed);
  }
}

static void
nyx_mpris_volume_changed (NyxFeature *feature, gdouble volume)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  gdouble mpris_volume;

  if (G_UNLIKELY (volume < 0))
    volume = 0;

  mpris_volume = nyx_mpris_media_player2_player_get_volume (self->player_skeleton);

  if (!G_APPROX_VALUE (volume, mpris_volume, FLT_EPSILON)) {
    GST_LOG_OBJECT (self, "Volume changed to: %lf", volume);
    nyx_mpris_media_player2_player_set_volume (self->player_skeleton, volume);
  }
}

static void
nyx_mpris_played_item_changed (NyxFeature *feature, NyxMediaItem *item)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  GVariant *variant = NULL;
  guint index = 0;

  GST_DEBUG_OBJECT (self, "Played item changed to: %" GST_PTR_FORMAT, item);

  if (G_LIKELY (_mpris_find_track_by_item (self, item, &index))) {
    self->current_track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);
    variant = _mpris_build_track_metadata (self, self->current_track);
  } else {
    self->current_track = NULL;
  }

  nyx_mpris_refresh_current_track (self, variant);
  nyx_mpris_refresh_can_go_next_previous (self);
}

static void
nyx_mpris_item_updated (NyxFeature *feature, NyxMediaItem *item)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  guint index = 0;

  GST_LOG_OBJECT (self, "Item updated: %" GST_PTR_FORMAT, item);

  if (_mpris_find_track_by_item (self, item, &index)) {
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);
    nyx_mpris_refresh_track (self, track);
  }
}

static void
nyx_mpris_queue_item_added (NyxFeature *feature, NyxMediaItem *item, guint index)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  NyxMprisTrack *track, *prev_track = NULL;
  GVariant *variant;

  /* Safety precaution for a case when someone adds MPRIS feature
   * in middle of altering playlist from another thread, since we
   * also read initial playlist after name is acquired. */
  if (G_UNLIKELY (_mpris_find_track_by_item (self, item, NULL)))
    return;

  GST_DEBUG_OBJECT (self, "Queue item added at position: %u", index);

  track = nyx_mpris_track_new (item);
  g_ptr_array_insert (self->tracks, index, track);

  nyx_mpris_refresh_track_list (self);
  nyx_mpris_refresh_can_go_next_previous (self);

  variant = _mpris_build_track_metadata (self, track);

  /* NoTrack when item is added at first position in queue */
  nyx_mpris_media_player2_track_list_emit_track_added (self->tracks_skeleton,
      variant, (prev_track != NULL) ? prev_track->id : NYX_MPRIS_NO_TRACK);
}

static void
nyx_mpris_queue_item_removed (NyxFeature *feature, NyxMediaItem *item, guint index)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  NyxMprisTrack *track;

  GST_DEBUG_OBJECT (self, "Queue item removed");

  track = (NyxMprisTrack *) g_ptr_array_steal_index (self->tracks, index);

  if (track == self->current_track) {
    self->current_track = NULL;
    nyx_mpris_refresh_current_track (self, NULL);
  }

  nyx_mpris_refresh_track_list (self);
  nyx_mpris_refresh_can_go_next_previous (self);
  nyx_mpris_media_player2_track_list_emit_track_removed (self->tracks_skeleton, track->id);

  nyx_mpris_track_free (track);
}

static void
nyx_mpris_queue_item_repositioned (NyxFeature *feature, guint before, guint after)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  NyxMprisTrack *track;

  GST_DEBUG_OBJECT (self, "Queue item repositioned: %u -> %u", before, after);

  track = (NyxMprisTrack *) g_ptr_array_steal_index (self->tracks, before);
  g_ptr_array_insert (self->tracks, after, track);

  nyx_mpris_refresh_track_list (self);
  nyx_mpris_refresh_can_go_next_previous (self);
}

static void
nyx_mpris_queue_cleared (NyxFeature *feature)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  guint n_items = self->tracks->len;

  if (n_items > 0)
    g_ptr_array_remove_range (self->tracks, 0, n_items);

  self->current_track = NULL;
  nyx_mpris_refresh_current_track (self, NULL);
  nyx_mpris_refresh_can_go_next_previous (self);
  nyx_mpris_refresh_track_list (self);

  nyx_mpris_media_player2_track_list_emit_track_list_replaced (self->tracks_skeleton,
      empty_tracklist, NYX_MPRIS_NO_TRACK);
}

static void
nyx_mpris_queue_progression_changed (NyxFeature *feature, NyxQueueProgressionMode mode)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  const gchar *loop_status = NYX_MPRIS_LOOP_NONE;
  gboolean shuffle = FALSE;

  GST_DEBUG_OBJECT (self, "Queue progression changed to: %i", mode);

  switch (mode) {
    case NYX_QUEUE_PROGRESSION_REPEAT_ITEM:
      loop_status = NYX_MPRIS_LOOP_TRACK;
      break;
    case NYX_QUEUE_PROGRESSION_CAROUSEL:
      loop_status = NYX_MPRIS_LOOP_PLAYLIST;
      break;
    case NYX_QUEUE_PROGRESSION_SHUFFLE:
      shuffle = TRUE;
      break;
    case NYX_QUEUE_PROGRESSION_NONE:
    case NYX_QUEUE_PROGRESSION_CONSECUTIVE:
      self->default_mode = mode;
      break;
    default:
      break;
  }

  if (mode != NYX_QUEUE_PROGRESSION_SHUFFLE)
    self->non_shuffle_mode = mode;

  nyx_mpris_media_player2_player_set_loop_status (self->player_skeleton, loop_status);
  nyx_mpris_media_player2_player_set_shuffle (self->player_skeleton, shuffle);
}

static gboolean
_handle_open_uri_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, const gchar *uri, NyxMpris *self)
{
  NyxPlayer *player;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  GST_DEBUG_OBJECT (self, "Handle open URI: %s", uri);

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    NyxMediaItem *item = nyx_media_item_new (uri);

    /* We can only alter NyxQueue from main thread.
     * Adding items to it will trigger nyx_mpris_queue_item_added(),
     * then we will add this new item to our track list */
    nyx_utils_queue_append_on_main_sync (queue, item);

    if (nyx_queue_select_item (queue, item))
      nyx_player_play (player);

    gst_object_unref (item);
  });

  nyx_mpris_media_player2_player_complete_open_uri (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_play_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle play");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    nyx_player_play (player);
  });

  nyx_mpris_media_player2_player_complete_play (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_pause_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle pause");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    nyx_player_pause (player);
  });

  nyx_mpris_media_player2_player_complete_pause (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_play_pause_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle play/pause");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxPlayerState state = nyx_player_get_state (player);

    switch (state) {
      case NYX_PLAYER_STATE_PLAYING:
        nyx_player_pause (player);
        break;
      case NYX_PLAYER_STATE_PAUSED:
      case NYX_PLAYER_STATE_STOPPED:
        nyx_player_play (player);
        break;
      default:
        break;
    }
  });

  nyx_mpris_media_player2_player_complete_play_pause (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_stop_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle stop");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    nyx_player_stop (player);
  });

  nyx_mpris_media_player2_player_complete_stop (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_next_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  GST_DEBUG_OBJECT (self, "Handle next");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    nyx_queue_select_next_item (queue);
  });

  nyx_mpris_media_player2_player_complete_next (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_previous_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, NyxMpris *self)
{
  NyxPlayer *player;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  GST_DEBUG_OBJECT (self, "Handle previous");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    nyx_queue_select_previous_item (queue);
  });

  nyx_mpris_media_player2_player_complete_previous (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_seek_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, gint64 offset, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle seek");

  if (!self->current_track)
    goto finish;

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    gdouble position, seek_position;

    position = nyx_player_get_position (player);
    seek_position = position + NYX_MPRIS_USECONDS_TO_SECONDS (offset);

    if (seek_position <= 0) {
      nyx_player_seek (player, 0);
    } else {
      gdouble duration = nyx_media_item_get_duration (self->current_track->item);

      if (seek_position > duration) {
        NyxQueue *queue = nyx_player_get_queue (player);
        nyx_queue_select_next_item (queue);
      } else {
        nyx_player_seek (player, seek_position);
      }
    }
  });

finish:
  nyx_mpris_media_player2_player_complete_seek (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_set_position_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id,
    gint64 position, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle set position");

  if (G_UNLIKELY (position < 0) || !self->current_track)
    goto finish;

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    gdouble duration, position_dbl;

    duration = nyx_media_item_get_duration (self->current_track->item);
    position_dbl = NYX_MPRIS_USECONDS_TO_SECONDS (position);

    if (position_dbl <= duration)
      nyx_player_seek (player, position_dbl);
  });

finish:
  nyx_mpris_media_player2_player_complete_set_position (player_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
_handle_rate_notify_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle rate notify");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    gdouble speed, player_speed;

    speed = nyx_mpris_media_player2_player_get_rate (player_skeleton);
    player_speed = nyx_player_get_speed (player);

    if (!G_APPROX_VALUE (speed, player_speed, FLT_EPSILON))
      nyx_player_set_speed (player, speed);
  });
}

static void
_handle_volume_notify_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle volume notify");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    gdouble volume, player_volume;

    volume = nyx_mpris_media_player2_player_get_volume (player_skeleton);
    player_volume = nyx_player_get_volume (player);

    if (!G_APPROX_VALUE (volume, player_volume, FLT_EPSILON))
      nyx_player_set_volume (player, volume);
  });
}

static void
_handle_loop_status_notify_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle loop status notify");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    NyxQueueProgressionMode mode, player_mode;
    const gchar *loop_status;

    loop_status = nyx_mpris_media_player2_player_get_loop_status (player_skeleton);
    player_mode = nyx_queue_get_progression_mode (queue);

    /* When in shuffle and no loop, assume default mode (none or consecutive).
     * This prevents us from getting stuck constantly changing loop and shuffle. */
    if (player_mode == NYX_QUEUE_PROGRESSION_SHUFFLE)
      player_mode = self->default_mode;

    mode = NYX_MPRIS_COMPARE (loop_status, NYX_MPRIS_LOOP_TRACK)
        ? NYX_QUEUE_PROGRESSION_REPEAT_ITEM
        : NYX_MPRIS_COMPARE (loop_status, NYX_MPRIS_LOOP_PLAYLIST)
        ? NYX_QUEUE_PROGRESSION_CAROUSEL
        : self->default_mode;

    if (mode != player_mode)
      nyx_queue_set_progression_mode (queue, mode);
  });
}

static void
_handle_shuffle_notify_cb (NyxMprisMediaPlayer2Player *player_skeleton,
    GParamSpec *pspec G_GNUC_UNUSED, NyxMpris *self)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Handle shuffle notify");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    NyxQueueProgressionMode player_mode;
    gboolean shuffle, player_shuffle;

    player_mode = nyx_queue_get_progression_mode (queue);

    shuffle = nyx_mpris_media_player2_player_get_shuffle (player_skeleton);
    player_shuffle = (player_mode == NYX_QUEUE_PROGRESSION_SHUFFLE);

    if (shuffle != player_shuffle) {
      nyx_queue_set_progression_mode (queue,
          (shuffle) ? NYX_QUEUE_PROGRESSION_SHUFFLE : self->non_shuffle_mode);
    }
  });
}

static gboolean
_handle_get_tracks_metadata_cb (NyxMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *const *tracks_ids, NyxMpris *self)
{
  GVariantBuilder builder;
  GVariant *tracks_variant = NULL;
  gboolean initialized = FALSE;
  guint i;

  GST_DEBUG_OBJECT (self, "Handle get tracks metadata");

  for (i = 0; tracks_ids[i]; ++i) {
    guint index = 0;

    if (_mpris_find_track_by_id (self, tracks_ids[i], &index)) {
      NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);
      GVariant *variant = _mpris_build_track_metadata (self, track);

      if (!initialized) {
        g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
        initialized = TRUE;
      }

      g_variant_builder_add_value (&builder, variant);
    }
  }

  if (initialized)
    tracks_variant = g_variant_builder_end (&builder);

  nyx_mpris_media_player2_track_list_complete_get_tracks_metadata (tracks_skeleton,
      invocation, tracks_variant);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_add_track_cb (NyxMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *uri,
    const gchar *after_track, gboolean set_current, NyxMpris *self)
{
  NyxPlayer *player;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  GST_DEBUG_OBJECT (self, "Handle add track, URI: %s, after_track: %s,"
      " set_current: %s", uri, after_track, set_current ? "yes" : "no");

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxMediaItem *after_item = NULL;
    gboolean add;

    if ((add = NYX_MPRIS_COMPARE (after_track, NYX_MPRIS_NO_TRACK))) {
      GST_DEBUG_OBJECT (self, "Prepend, since requested after \"NoTrack\"");
    } else {
      guint index = 0;

      if ((add = _mpris_find_track_by_id (self, after_track, &index))) {
        NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);

        GST_DEBUG_OBJECT (self, "Append after: %s", track->id);
        after_item = track->item;
      }
    }

    if (add) {
      NyxQueue *queue = nyx_player_get_queue (player);
      NyxMediaItem *item = nyx_media_item_new (uri);

      nyx_utils_queue_insert_on_main_sync (queue, item, after_item);

      if (set_current && nyx_queue_select_item (queue, item))
          nyx_player_play (player);

      gst_object_unref (item);
    }
  });

  nyx_mpris_media_player2_track_list_complete_add_track (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_remove_track_cb (NyxMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id, NyxMpris *self)
{
  NyxPlayer *player;
  guint index = 0;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  GST_DEBUG_OBJECT (self, "Handle remove track");

  if (!_mpris_find_track_by_id (self, track_id, &index))
    goto finish;

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);

    nyx_utils_queue_remove_on_main_sync (queue, track->item);
  });

finish:
  nyx_mpris_media_player2_track_list_complete_remove_track (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
_handle_go_to_cb (NyxMprisMediaPlayer2TrackList *tracks_skeleton,
    GDBusMethodInvocation *invocation, const gchar *track_id, NyxMpris *self)
{
  NyxPlayer *player;
  guint index = 0;

  if (!nyx_mpris_get_queue_controllable (self))
    return G_DBUS_METHOD_INVOCATION_UNHANDLED;

  if (!_mpris_find_track_by_id (self, track_id, &index))
    goto finish;

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    NyxMprisTrack *track = (NyxMprisTrack *) g_ptr_array_index (self->tracks, index);

    if (nyx_queue_select_item (queue, track->item))
      nyx_player_play (player);
  });

finish:
  nyx_mpris_media_player2_track_list_complete_go_to (tracks_skeleton, invocation);

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
_name_acquired_cb (GDBusConnection *connection, const gchar *name, NyxMpris *self)
{
  NyxPlayer *player;
  GError *error = NULL;
  gchar **uri_schemes, **mime_types;

  GST_DEBUG_OBJECT (self, "Name acquired: %s", name);

  if (!(self->base_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->base_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }
  if (!(self->player_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->player_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }
  if (!(self->tracks_exported = g_dbus_interface_skeleton_export (
      G_DBUS_INTERFACE_SKELETON (self->tracks_skeleton),
      connection, "/org/mpris/MediaPlayer2", &error))) {
    goto finish;
  }

  self->registered = TRUE;

  nyx_mpris_media_player2_set_identity (self->base_skeleton, self->identity);
  nyx_mpris_media_player2_set_desktop_entry (self->base_skeleton, self->desktop_entry);

  uri_schemes = nyx_mpris_get_supported_uri_schemes (self);
  nyx_mpris_media_player2_set_supported_uri_schemes (self->base_skeleton,
      (const gchar *const *) uri_schemes);
  g_strfreev (uri_schemes);

  mime_types = nyx_mpris_get_supported_mime_types (self);
  nyx_mpris_media_player2_set_supported_mime_types (self->base_skeleton,
      (const gchar *const *) mime_types);
  g_strfreev (mime_types);

  /* As stated in MPRIS docs: "This property is not expected to change,
   * as it describes an intrinsic capability of the implementation." */
  nyx_mpris_media_player2_player_set_can_control (self->player_skeleton, TRUE);
  nyx_mpris_media_player2_set_has_track_list (self->base_skeleton, TRUE);
  nyx_mpris_media_player2_track_list_set_can_edit_tracks (self->tracks_skeleton,
      nyx_mpris_get_queue_controllable (self));

  NYX_MPRIS_DO_WITH_PLAYER (self, &player, {
    NyxQueue *queue = nyx_player_get_queue (player);
    GVariant *variant = NULL;

    _mpris_read_initial_tracks (self, queue);

    /* Update tracks IDs after reading initial tracks from queue */
    nyx_mpris_refresh_track_list (self);

    if (self->current_track)
      variant = _mpris_build_track_metadata (self, self->current_track);

    nyx_mpris_refresh_current_track (self, variant);
    nyx_mpris_refresh_can_go_next_previous (self);

    /* Set some initial default progressions to revert to and
     * try to update them in progression_changed call below */
    self->default_mode = NYX_QUEUE_PROGRESSION_NONE;
    self->non_shuffle_mode = NYX_QUEUE_PROGRESSION_NONE;

    /* Trigger update with current values */
    nyx_mpris_state_changed (NYX_FEATURE (self), nyx_player_get_state (player));
    nyx_mpris_position_changed (NYX_FEATURE (self), nyx_player_get_position (player));
    nyx_mpris_speed_changed (NYX_FEATURE (self), nyx_player_get_speed (player));
    nyx_mpris_volume_changed (NYX_FEATURE (self), nyx_player_get_volume (player));
    nyx_mpris_queue_progression_changed (NYX_FEATURE (self), nyx_queue_get_progression_mode (queue));
  });

finish:
  if (error) {
    GST_ERROR_OBJECT (self, "Error: %s", (error && error->message)
        ? error->message : "Unknown DBUS error occured");
    g_error_free (error);

    nyx_mpris_unregister (self);
  }

  if (self->loop && g_main_loop_is_running (self->loop))
    g_main_loop_quit (self->loop);
}

static void
_name_lost_cb (GDBusConnection *connection, const gchar *name, NyxMpris *self)
{
  GST_DEBUG_OBJECT (self, "Name lost: %s", name);

  if (self->loop && g_main_loop_is_running (self->loop))
    g_main_loop_quit (self->loop);

  nyx_mpris_unregister (self);
}

static gboolean
nyx_mpris_prepare (NyxFeature *feature)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);
  GDBusConnection *connection;
  gchar *address;

  GST_DEBUG_OBJECT (self, "Prepare");

  if (!(address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, NULL))) {
    GST_WARNING_OBJECT (self, "No MPRIS bus address");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Obtained MPRIS DBus address: %s", address);

  connection = g_dbus_connection_new_for_address_sync (address,
      G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT
      | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, NULL);
  g_free (address);

  if (!connection) {
    GST_WARNING_OBJECT (self, "No MPRIS bus connection");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Obtained MPRIS DBus connection");

  self->loop = g_main_loop_new (g_main_context_get_thread_default (), FALSE);

  self->name_id = g_bus_own_name_on_connection (connection, self->own_name,
      G_BUS_NAME_OWNER_FLAGS_NONE,
      (GBusNameAcquiredCallback) _name_acquired_cb,
      (GBusNameLostCallback) _name_lost_cb,
      self, NULL);
  g_object_unref (connection);

  /* Wait until connection is established */
  g_main_loop_run (self->loop);
  g_clear_pointer (&self->loop, g_main_loop_unref);

  if (self->registered) {
    GST_DEBUG_OBJECT (self, "Own name ID: %u", self->name_id);
  } else if (self->name_id > 0) {
    GST_ERROR_OBJECT (self, "Could not register MPRIS connection");
    g_bus_unown_name (self->name_id);
    self->name_id = 0;
  }

  return self->registered;
}

static gboolean
nyx_mpris_unprepare (NyxFeature *feature)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  nyx_mpris_unregister (self);

  if (self->name_id > 0) {
    g_bus_unown_name (self->name_id);
    self->name_id = 0;
  }

  return TRUE;
}

static void
nyx_mpris_property_changed (NyxFeature *feature, GParamSpec *pspec)
{
  NyxMpris *self = NYX_MPRIS_CAST (feature);

  GST_DEBUG_OBJECT (self, "Property changed: \"%s\"",
      g_param_spec_get_name (pspec));

  if (pspec == param_specs[PROP_FALLBACK_ART_URL]) {
    nyx_mpris_refresh_all_tracks (self);
  } else if (pspec == param_specs[PROP_QUEUE_CONTROLLABLE]) {
    nyx_mpris_media_player2_track_list_set_can_edit_tracks (self->tracks_skeleton,
        nyx_mpris_get_queue_controllable (self));
    nyx_mpris_refresh_can_go_next_previous (self);
  }
}

/**
 * nyx_mpris_new:
 * @own_name: an unique DBus name with "org.mpris.MediaPlayer2." prefix
 * @identity: a media player friendly name
 * @desktop_entry: (nullable): desktop file basename (without ".desktop" extension)
 *
 * Creates a new #NyxMpris instance.
 *
 * Returns: (transfer full): a new #NyxMpris instance.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */
NyxMpris *
nyx_mpris_new (const gchar *own_name, const gchar *identity,
    const gchar *desktop_entry)
{
  NyxMpris *mpris;

  mpris = g_object_new (NYX_TYPE_MPRIS,
      "own-name", own_name,
      "identity", identity,
      "desktop-entry", desktop_entry,
      NULL);
  gst_object_ref_sink (mpris);

  return mpris;
}

/**
 * nyx_mpris_set_queue_controllable:
 * @mpris: a #NyxMpris
 * @controllable: if #NyxQueue should be controllable
 *
 * Set whether remote MPRIS clients can control #NyxQueue.
 *
 * This includes ability to open new URIs, adding/removing
 * items from the queue and selecting current item for
 * playback remotely using MPRIS interface.
 *
 * You probably want to keep this disabled if your application
 * is supposed to manage what is played now and not MPRIS client.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */
void
nyx_mpris_set_queue_controllable (NyxMpris *self, gboolean controllable)
{
  gboolean prev_controllable;

  g_return_if_fail (NYX_IS_MPRIS (self));

  prev_controllable = (gboolean) g_atomic_int_exchange (&self->queue_controllable, (gint) controllable);

  if (prev_controllable != controllable)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_QUEUE_CONTROLLABLE]);
}

/**
 * nyx_mpris_get_queue_controllable:
 * @mpris: a #NyxMpris
 *
 * Get whether remote `MPRIS` clients can control [class@Nyx.Queue].
 *
 * Returns: %TRUE if control over #NyxQueue is allowed, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */
gboolean
nyx_mpris_get_queue_controllable (NyxMpris *self)
{
  g_return_val_if_fail (NYX_IS_MPRIS (self), FALSE);

  return (gboolean) g_atomic_int_get (&self->queue_controllable);
}

/**
 * nyx_mpris_set_fallback_art_url:
 * @mpris: a #NyxMpris
 * @art_url: (nullable): an art URL
 *
 * Set fallback artwork to show when media does not provide one.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */
void
nyx_mpris_set_fallback_art_url (NyxMpris *self, const gchar *art_url)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_MPRIS (self));

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->fallback_art_url, art_url);
  GST_OBJECT_UNLOCK (self);

  if (changed)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_FALLBACK_ART_URL]);
}

/**
 * nyx_mpris_get_fallback_art_url:
 * @mpris: a #NyxMpris
 *
 * Get fallback art URL earlier set by user.
 *
 * Returns: (transfer full) (nullable): fallback art URL.
 *
 * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
 */
gchar *
nyx_mpris_get_fallback_art_url (NyxMpris *self)
{
  gchar *art_url;

  g_return_val_if_fail (NYX_IS_MPRIS (self), NULL);

  GST_OBJECT_LOCK (self);
  art_url = g_strdup (self->fallback_art_url);
  GST_OBJECT_UNLOCK (self);

  return art_url;
}

static void
nyx_mpris_init (NyxMpris *self)
{
  self->base_skeleton = nyx_mpris_media_player2_skeleton_new ();
  self->player_skeleton = nyx_mpris_media_player2_player_skeleton_new ();
  self->tracks_skeleton = nyx_mpris_media_player2_track_list_skeleton_new ();

  self->tracks = g_ptr_array_new_with_free_func ((GDestroyNotify) nyx_mpris_track_free);

  g_atomic_int_set (&self->queue_controllable, (gint) DEFAULT_QUEUE_CONTROLLABLE);

  g_signal_connect (self->player_skeleton, "handle-open-uri",
      G_CALLBACK (_handle_open_uri_cb), self);
  g_signal_connect (self->player_skeleton, "handle-play",
      G_CALLBACK (_handle_play_cb), self);
  g_signal_connect (self->player_skeleton, "handle-pause",
      G_CALLBACK (_handle_pause_cb), self);
  g_signal_connect (self->player_skeleton, "handle-play-pause",
      G_CALLBACK (_handle_play_pause_cb), self);
  g_signal_connect (self->player_skeleton, "handle-stop",
      G_CALLBACK (_handle_stop_cb), self);
  g_signal_connect (self->player_skeleton, "handle-next",
      G_CALLBACK (_handle_next_cb), self);
  g_signal_connect (self->player_skeleton, "handle-previous",
      G_CALLBACK (_handle_previous_cb), self);
  g_signal_connect (self->player_skeleton, "handle-seek",
      G_CALLBACK (_handle_seek_cb), self);
  g_signal_connect (self->player_skeleton, "handle-set-position",
      G_CALLBACK (_handle_set_position_cb), self);
  g_signal_connect (self->player_skeleton, "notify::rate",
      G_CALLBACK (_handle_rate_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::volume",
      G_CALLBACK (_handle_volume_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::loop-status",
      G_CALLBACK (_handle_loop_status_notify_cb), self);
  g_signal_connect (self->player_skeleton, "notify::shuffle",
      G_CALLBACK (_handle_shuffle_notify_cb), self);

  g_signal_connect (self->tracks_skeleton, "handle-get-tracks-metadata",
      G_CALLBACK (_handle_get_tracks_metadata_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-add-track",
      G_CALLBACK (_handle_add_track_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-remove-track",
      G_CALLBACK (_handle_remove_track_cb), self);
  g_signal_connect (self->tracks_skeleton, "handle-go-to",
      G_CALLBACK (_handle_go_to_cb), self);
}

static void
nyx_mpris_finalize (GObject *object)
{
  NyxMpris *self = NYX_MPRIS_CAST (object);

  g_object_unref (self->base_skeleton);
  g_object_unref (self->player_skeleton);
  g_object_unref (self->tracks_skeleton);

  self->current_track = NULL;
  g_ptr_array_unref (self->tracks);

  g_free (self->own_name);
  g_free (self->identity);
  g_free (self->desktop_entry);
  g_free (self->fallback_art_url);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_mpris_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxMpris *self = NYX_MPRIS_CAST (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      self->own_name = g_value_dup_string (value);
      break;
    case PROP_IDENTITY:
      self->identity = g_value_dup_string (value);
      break;
    case PROP_DESKTOP_ENTRY:
      self->desktop_entry = g_value_dup_string (value);
      break;
    case PROP_QUEUE_CONTROLLABLE:
      nyx_mpris_set_queue_controllable (self, g_value_get_boolean (value));
      break;
    case PROP_FALLBACK_ART_URL:
      nyx_mpris_set_fallback_art_url (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_mpris_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxMpris *self = NYX_MPRIS_CAST (object);

  switch (prop_id) {
    case PROP_OWN_NAME:
      g_value_set_string (value, self->own_name);
      break;
    case PROP_IDENTITY:
      g_value_set_string (value, self->identity);
      break;
    case PROP_DESKTOP_ENTRY:
      g_value_set_string (value, self->desktop_entry);
      break;
    case PROP_QUEUE_CONTROLLABLE:
      g_value_set_boolean (value, nyx_mpris_get_queue_controllable (self));
      break;
    case PROP_FALLBACK_ART_URL:
      g_value_take_string (value, nyx_mpris_get_fallback_art_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_mpris_class_init (NyxMprisClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxFeatureClass *feature_class = (NyxFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxmpris", 0,
      "Nyx Mpris");

  gobject_class->get_property = nyx_mpris_get_property;
  gobject_class->set_property = nyx_mpris_set_property;
  gobject_class->finalize = nyx_mpris_finalize;

  feature_class->prepare = nyx_mpris_prepare;
  feature_class->unprepare = nyx_mpris_unprepare;
  feature_class->property_changed = nyx_mpris_property_changed;
  feature_class->state_changed = nyx_mpris_state_changed;
  feature_class->position_changed = nyx_mpris_position_changed;
  feature_class->speed_changed = nyx_mpris_speed_changed;
  feature_class->volume_changed = nyx_mpris_volume_changed;
  feature_class->played_item_changed = nyx_mpris_played_item_changed;
  feature_class->item_updated = nyx_mpris_item_updated;
  feature_class->queue_item_added = nyx_mpris_queue_item_added;
  feature_class->queue_item_removed = nyx_mpris_queue_item_removed;
  feature_class->queue_item_repositioned = nyx_mpris_queue_item_repositioned;
  feature_class->queue_cleared = nyx_mpris_queue_cleared;
  feature_class->queue_progression_changed = nyx_mpris_queue_progression_changed;

  /**
   * NyxMpris:own-name:
   *
   * DBus name to own on connection.
   *
   * Must be written as a reverse DNS format starting with "org.mpris.MediaPlayer2." prefix.
   * Each #NyxMpris instance running on the same system must have an unique name.
   *
   * Example: "org.mpris.MediaPlayer2.MyPlayer.instance123"
   *
   * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_OWN_NAME] = g_param_spec_string ("own-name",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMpris:identity:
   *
   * A friendly name to identify the media player.
   *
   * Example: "My Player"
   *
   * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_IDENTITY] = g_param_spec_string ("identity",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMpris:desktop-entry:
   *
   * The basename of an installed .desktop file with the ".desktop" extension stripped.
   *
   * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_DESKTOP_ENTRY] = g_param_spec_string ("desktop-entry",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMpris:queue-controllable:
   *
   * Whether remote MPRIS clients can control #NyxQueue.
   *
   * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_QUEUE_CONTROLLABLE] = g_param_spec_boolean ("queue-controllable",
      NULL, NULL, DEFAULT_QUEUE_CONTROLLABLE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMpris:fallback-art-url:
   *
   * Fallback artwork to show when media does not provide one.
   *
   * Deprecated: 0.10: Use MPRIS from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_FALLBACK_ART_URL] = g_param_spec_string ("fallback-art-url",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
