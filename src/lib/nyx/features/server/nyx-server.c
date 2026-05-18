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
 * NyxServer:
 *
 * An optional Server feature to add to the player.
 *
 * #NyxServer is a feature that hosts a local server
 * providing an ability to both monitor and control playback
 * through WebSocket messages and HTTP requests.
 *
 * Use [const@Nyx.HAVE_SERVER] macro to check if Nyx API
 * was compiled with this feature.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */

#include <gst/gst.h>
#include <libsoup/soup.h>

#include "nyx-server.h"
#include "nyx-server-names-private.h"
#include "nyx-server-json-private.h"
#include "nyx-server-actions-private.h"
#include "nyx-server-mdns-private.h"

#include "nyx-enums.h"
#include "nyx-player.h"
#include "nyx-queue.h"
#include "nyx-media-item.h"
#include "nyx-utils-private.h"
#include "../shared/nyx-shared-utils-private.h"

#define NYX_SERVER_WS_EVENT_STATE_MAKE(s) (NYX_SERVER_WS_EVENT_STATE " " s)
#define NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE(s) (NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION " " s)

#define DOUBLE_EVENT_MAX_SIZE 12
#define UINT_EVENT_MAX_SIZE 24

#define PORT_MAX 65535

#define DEFAULT_ENABLED FALSE
#define DEFAULT_QUEUE_CONTROLLABLE FALSE

#define GST_CAT_DEFAULT nyx_server_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxServer
{
  NyxFeature parent;

  SoupServer *server;

  GPtrArray *ws_connections;

  GPtrArray *items;
  NyxMediaItem *played_item;
  guint played_index;

  guint position_uint;

  guint error_id;
  guint running_id;
  GSource *timeout_source;

  gint enabled; // atomic
  gboolean running; // only changed from features thread
  guint port;
  guint current_port;
  gint queue_controllable; // atomic
};

typedef struct
{
  NyxServer *server;
  GError *error;
} NyxServerErrorData;

enum
{
  PROP_0,
  PROP_ENABLED,
  PROP_RUNNING,
  PROP_PORT,
  PROP_CURRENT_PORT,
  PROP_QUEUE_CONTROLLABLE,
  PROP_LAST
};

enum
{
  SIGNAL_ERROR,
  SIGNAL_LAST
};

#define parent_class nyx_server_parent_class
G_DEFINE_TYPE (NyxServer, nyx_server, NYX_TYPE_FEATURE);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static NyxServerErrorData *
nyx_server_error_data_new (NyxServer *self, GError *error)
{
  NyxServerErrorData *data = g_new (NyxServerErrorData, 1);

  GST_TRACE ("Created server error data: %p", data);

  data->server = gst_object_ref (self);
  data->error = error;

  return data;
}

static void
nyx_server_error_data_free (NyxServerErrorData *data)
{
  GST_TRACE ("Freeing server error data: %p", data);

  gst_object_unref (data->server);
  g_clear_error (&data->error);

  g_free (data);
}

static void
nyx_server_notify_port_and_running_on_main_idle (NyxServer *self)
{
  GST_OBJECT_LOCK (self);
  self->running_id = 0;
  GST_OBJECT_UNLOCK (self);

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_PORT]);
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_RUNNING]);
}

static void
nyx_server_send_error_on_main_idle (NyxServerErrorData *data)
{
  NyxServer *self = data->server;

  GST_OBJECT_LOCK (self);
  self->error_id = 0;
  GST_OBJECT_UNLOCK (self);

  g_signal_emit (G_OBJECT (self), signals[SIGNAL_ERROR], 0, data->error);
}

static inline void
_clear_delayed_queue_changed_timeout (NyxServer *self)
{
  if (self->timeout_source) {
    g_source_destroy (self->timeout_source);
    g_clear_pointer (&self->timeout_source, g_source_unref);
  }
}

static inline guint
_find_current_port (NyxServer *self)
{
  GSList *uris_list, *list;
  guint found_port = 0;

  uris_list = soup_server_get_uris (self->server);

  for (list = uris_list; list != NULL; list = g_slist_next (list)) {
    GUri *uri = list->data;
    gint current_port = g_uri_get_port (uri);

    if (current_port > 0) {
      found_port = current_port;
      break;
    }
  }

  g_slist_free_full (uris_list, (GDestroyNotify) g_uri_unref);

  if (G_UNLIKELY (found_port == 0))
    GST_ERROR_OBJECT (self, "Could not determine server current port");

  return found_port;
}

static inline void
_start_server (NyxServer *self)
{
  GError *error = NULL;
  guint current_port;

  /* We only edit this from feature thread,
   * so no lock needed here */
  if (self->running)
    return;

  if (!soup_server_listen_all (self->server,
      nyx_server_get_port (self),
      SOUP_SERVER_LISTEN_IPV4_ONLY,
      &error)) {
    NyxServerErrorData *data;

    GST_ERROR_OBJECT (self, "Error starting server: %s",
        GST_STR_NULL (error->message));

    data = nyx_server_error_data_new (self, error);

    GST_OBJECT_LOCK (self);
    g_clear_handle_id (&self->error_id, g_source_remove);
    self->error_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
        (GSourceFunc) nyx_server_send_error_on_main_idle, data,
        (GDestroyNotify) nyx_server_error_data_free);
    GST_OBJECT_UNLOCK (self);

    return;
  }

  current_port = _find_current_port (self);
  GST_INFO_OBJECT (self, "Server started on port: %u", current_port);

  GST_OBJECT_LOCK (self);

  self->current_port = current_port;
  self->running = TRUE;

  g_clear_handle_id (&self->running_id, g_source_remove);
  self->running_id = g_idle_add_once (
      (GSourceOnceFunc) nyx_server_notify_port_and_running_on_main_idle, self);

  GST_OBJECT_UNLOCK (self);

  nyx_server_mdns_serve (
      gst_object_get_name (GST_OBJECT_CAST (self)), current_port);
}

static inline void
_stop_server (NyxServer *self)
{
  guint current_port;

  if (!self->running)
    return;

  _clear_delayed_queue_changed_timeout (self);

  GST_OBJECT_LOCK (self);

  current_port = self->current_port;
  self->current_port = 0;
  self->running = FALSE;

  g_clear_handle_id (&self->running_id, g_source_remove);
  self->running_id = g_idle_add_once (
      (GSourceOnceFunc) nyx_server_notify_port_and_running_on_main_idle, self);

  GST_OBJECT_UNLOCK (self);

  nyx_server_mdns_remove (current_port);

  /* Remove everyone */
  if (self->ws_connections->len > 0)
    g_ptr_array_remove_range (self->ws_connections, 0, self->ws_connections->len);

  soup_server_disconnect (self->server);
  GST_INFO_OBJECT (self, "Server stopped listening");
}

static void
_clear_stored_queue (NyxServer *self)
{
  if (self->items->len > 0)
    g_ptr_array_remove_range (self->items, 0, self->items->len);

  gst_clear_object (&self->played_item);
  self->played_index = NYX_QUEUE_INVALID_POSITION;
}

static void
_ws_connection_message_cb (SoupWebsocketConnection *connection,
    gint type, GBytes *message, NyxServer *self)
{
  NyxPlayer *player;
  NyxServerAction action;
  const gchar *text;

  if (G_UNLIKELY (type != SOUP_WEBSOCKET_DATA_TEXT)) {
    GST_WARNING_OBJECT (self, "Received WS message with non-text data!");
    return;
  }

  text = g_bytes_get_data (message, NULL);

  if (G_UNLIKELY (text == NULL)) {
    GST_WARNING_OBJECT (self, "Received WS message without any text!");
    return;
  }

  action = nyx_server_actions_get_action (text);

  if (action == NYX_SERVER_ACTION_INVALID) {
    GST_INFO_OBJECT (self, "Ignoring WS message with invalid action text");
    return;
  }

  player = NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));

  if (G_UNLIKELY (player == NULL))
    return;

  switch (action) {
    case NYX_SERVER_ACTION_TOGGLE_PLAY:
      switch (nyx_player_get_state (player)) {
        case NYX_PLAYER_STATE_STOPPED:
        case NYX_PLAYER_STATE_PAUSED:
          nyx_player_play (player);
          break;
        case NYX_PLAYER_STATE_PLAYING:
          nyx_player_pause (player);
          break;
        default:
          break;
      }
      break;
    case NYX_SERVER_ACTION_PLAY:
      nyx_player_play (player);
      break;
    case NYX_SERVER_ACTION_PAUSE:
      nyx_player_pause (player);
      break;
    case NYX_SERVER_ACTION_STOP:
      nyx_player_stop (player);
      break;
    case NYX_SERVER_ACTION_SEEK:{
      gdouble position;
      if (nyx_server_actions_parse_seek (text, &position))
        nyx_player_seek (player, position);
      break;
    }
    case NYX_SERVER_ACTION_SET_SPEED:{
      gdouble speed;
      if (nyx_server_actions_parse_set_speed (text, &speed))
        nyx_player_set_speed (player, speed);
      break;
    }
    case NYX_SERVER_ACTION_SET_VOLUME:{
      gdouble volume;
      if (nyx_server_actions_parse_set_volume (text, &volume))
        nyx_player_set_volume (player, volume);
      break;
    }
    case NYX_SERVER_ACTION_SET_MUTE:{
      gboolean mute;
      if (nyx_server_actions_parse_set_mute (text, &mute))
        nyx_player_set_mute (player, mute);
      break;
    }
    case NYX_SERVER_ACTION_SET_PROGRESSION:{
      NyxQueueProgressionMode mode;
      if (nyx_server_actions_parse_set_progression (text, &mode))
        nyx_queue_set_progression_mode (nyx_player_get_queue (player), mode);
      break;
    }
    case NYX_SERVER_ACTION_ADD:{
      const gchar *uri;
      if (nyx_server_get_queue_controllable (self)
          && nyx_server_actions_parse_add (text, &uri)) {
        NyxMediaItem *item = nyx_media_item_new (uri);
        nyx_utils_queue_append_on_main_sync (nyx_player_get_queue (player), item);
        gst_object_unref (item);
      }
      break;
    }
    case NYX_SERVER_ACTION_INSERT:{
      gchar *uri;
      guint after_id;
      if (nyx_server_get_queue_controllable (self)
          && nyx_server_actions_parse_insert (text, &uri, &after_id)) {
        NyxMediaItem *item, *after_item = NULL;
        guint i;
        for (i = 0; i < self->items->len; ++i) {
          NyxMediaItem *tmp_after_item = (NyxMediaItem *) g_ptr_array_index (self->items, i);
          if (after_id == nyx_media_item_get_id (tmp_after_item)) {
            after_item = tmp_after_item;
            break;
          }
        }
        item = nyx_media_item_new (uri);
        nyx_utils_queue_insert_on_main_sync (nyx_player_get_queue (player), item, after_item);
        gst_object_unref (item);
        g_free (uri);
      }
      break;
    }
    case NYX_SERVER_ACTION_SELECT:{
      guint id;
      if (nyx_server_get_queue_controllable (self)
          && nyx_server_actions_parse_select (text, &id)) {
        guint i;
        for (i = 0; i < self->items->len; ++i) {
          NyxMediaItem *item = (NyxMediaItem *) g_ptr_array_index (self->items, i);
          if (id == nyx_media_item_get_id (item)) {
            nyx_queue_select_item (nyx_player_get_queue (player), item);
            break;
          }
        }
      }
      break;
    }
    case NYX_SERVER_ACTION_REMOVE:{
      guint id;
      if (nyx_server_get_queue_controllable (self)
          && nyx_server_actions_parse_remove (text, &id)) {
        guint i;
        for (i = 0; i < self->items->len; ++i) {
          NyxMediaItem *item = (NyxMediaItem *) g_ptr_array_index (self->items, i);
          if (id == nyx_media_item_get_id (item)) {
            nyx_utils_queue_remove_on_main_sync (nyx_player_get_queue (player), item);
            break;
          }
        }
      }
      break;
    }
    case NYX_SERVER_ACTION_CLEAR:
      if (nyx_server_get_queue_controllable (self))
        nyx_utils_queue_clear_on_main_sync (nyx_player_get_queue (player));
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  gst_object_unref (player);
}

static void
_ws_connection_closed_cb (SoupWebsocketConnection *connection, NyxServer *self)
{
  GST_INFO_OBJECT (self, "WebSocket connection closed: %p", connection);
  g_ptr_array_remove (self->ws_connections, connection);
}

static void
_request_cb (SoupServer *server, SoupServerMessage *msg,
    const gchar *path, GHashTable *query, NyxServer *self)
{
  gchar *data;

  if (!(data = nyx_server_json_build_complete (self,
      self->played_item, self->played_index, self->items))) {
    soup_server_message_set_status (msg, SOUP_STATUS_SERVICE_UNAVAILABLE, NULL);
    return;
  }

  soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
  soup_server_message_set_response (msg, "application/json",
      SOUP_MEMORY_TAKE, data, strlen (data));
}

static void
_websocket_connection_cb (SoupServer *server, SoupServerMessage *msg,
    const gchar *path, SoupWebsocketConnection *connection, NyxServer *self)
{
  GST_INFO_OBJECT (self, "New WebSocket connection: %p", connection);

  g_signal_connect (connection, "message", G_CALLBACK (_ws_connection_message_cb), self);
  g_signal_connect (connection, "closed", G_CALLBACK (_ws_connection_closed_cb), self);
  g_ptr_array_add (self->ws_connections, g_object_ref (connection));
}

static void
nyx_server_send_ws_message (NyxServer *self, const gchar *text)
{
  guint i;

  GST_LOG_OBJECT (self, "Sending WS message to clients: \"%s\"", text);

  for (i = 0; i < self->ws_connections->len; ++i) {
    SoupWebsocketConnection *connection = g_ptr_array_index (self->ws_connections, i);

    if (soup_websocket_connection_get_state (connection) != SOUP_WEBSOCKET_STATE_OPEN)
      continue;

    soup_websocket_connection_send_text (connection, text);
  }
}

static inline void
nyx_server_send_ws_double_event (NyxServer *self, const gchar *event, gdouble val)
{
  gchar text[DOUBLE_EVENT_MAX_SIZE];

  g_snprintf (text, sizeof (text), "%s %.2lf", event, val);
  nyx_server_send_ws_message (self, text);
}

static inline void
nyx_server_send_ws_uint_event (NyxServer *self, const gchar *event, guint val)
{
  gchar text[UINT_EVENT_MAX_SIZE];

  g_snprintf (text, sizeof (text), "%s %u", event, val);
  nyx_server_send_ws_message (self, text);
}

static gboolean
_send_ws_queue_changed_delayed_cb (NyxServer *self)
{
  GST_DEBUG_OBJECT (self, "Delayed queue changed handler reached");

  _clear_delayed_queue_changed_timeout (self);
  nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);

  return G_SOURCE_REMOVE;
}

static void
nyx_server_state_changed (NyxFeature *feature, NyxPlayerState state)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "State changed to: %i", state);

  if (!self->running || self->ws_connections->len == 0)
    return;

  switch (state) {
    case NYX_PLAYER_STATE_PLAYING:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_STATE_MAKE (NYX_SERVER_PLAYER_STATE_PLAYING));
      break;
    case NYX_PLAYER_STATE_PAUSED:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_STATE_MAKE (NYX_SERVER_PLAYER_STATE_PAUSED));
      break;
    case NYX_PLAYER_STATE_BUFFERING:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_STATE_MAKE (NYX_SERVER_PLAYER_STATE_BUFFERING));
      break;
    case NYX_PLAYER_STATE_STOPPED:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_STATE_MAKE (NYX_SERVER_PLAYER_STATE_STOPPED));
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
nyx_server_position_changed (NyxFeature *feature, gdouble position)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  /* Limit to seconds */
  if (ABS (self->position_uint - position) < 1)
    return;

  self->position_uint = (guint) position;
  GST_LOG_OBJECT (self, "Position changed to: %u", self->position_uint);

  if (self->running && self->ws_connections->len > 0)
    nyx_server_send_ws_uint_event (self, NYX_SERVER_WS_EVENT_POSITION, position);
}

static void
nyx_server_speed_changed (NyxFeature *feature, gdouble speed)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_LOG_OBJECT (self, "Speed changed to: %lf", speed);

  if (self->running && self->ws_connections->len > 0)
    nyx_server_send_ws_double_event (self, NYX_SERVER_WS_EVENT_SPEED, speed);
}

static void
nyx_server_volume_changed (NyxFeature *feature, gdouble volume)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_LOG_OBJECT (self, "Volume changed to: %lf", volume);

  if (self->running && self->ws_connections->len > 0)
    nyx_server_send_ws_double_event (self, NYX_SERVER_WS_EVENT_VOLUME, volume);
}

static void
nyx_server_mute_changed (NyxFeature *feature, gboolean mute)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  if (self->running && self->ws_connections->len > 0) {
    nyx_server_send_ws_message (self,
        (mute) ? NYX_SERVER_WS_EVENT_MUTED : NYX_SERVER_WS_EVENT_UNMUTED);
  }
}

static void
nyx_server_played_item_changed (NyxFeature *feature, NyxMediaItem *item)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Played item changed to: %" GST_PTR_FORMAT, item);

  gst_object_replace ((GstObject **) &self->played_item, GST_OBJECT_CAST (item));
  if (!g_ptr_array_find (self->items, self->played_item, &self->played_index))
    self->played_index = NYX_QUEUE_INVALID_POSITION;

  if (self->running && self->ws_connections->len > 0)
    nyx_server_send_ws_uint_event (self, NYX_SERVER_WS_EVENT_PLAYED_INDEX, self->played_index);
}

static void
nyx_server_item_updated (NyxFeature *feature, NyxMediaItem *item)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_LOG_OBJECT (self, "Item updated: %" GST_PTR_FORMAT, item);

  if (!self->running || self->ws_connections->len == 0)
    return;

  /* Clear timeout, since we will either send
   * immediately or set same timeout again */
  _clear_delayed_queue_changed_timeout (self);

  if (item != self->played_item) {
    /* Happens once per item when discovered, so send immediately */
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  } else {
    /* Current item can be updated very often (when bitrate changes)
     * so reduce amount of work here by adding delay */
    self->timeout_source = nyx_shared_utils_context_timeout_add_full (
        g_main_context_get_thread_default (),
        G_PRIORITY_DEFAULT_IDLE, 1000,
        (GSourceFunc) _send_ws_queue_changed_delayed_cb,
        self, NULL);
  }
}

static void
nyx_server_queue_item_added (NyxFeature *feature, NyxMediaItem *item, guint index)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item added %" GST_PTR_FORMAT, item);
  g_ptr_array_insert (self->items, index, gst_object_ref (item));

  if (self->running && self->ws_connections->len > 0) {
    _clear_delayed_queue_changed_timeout (self);
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  }
}

static void
nyx_server_queue_item_removed (NyxFeature *feature, NyxMediaItem *item, guint index)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue item removed %" GST_PTR_FORMAT, item);

  if (item == self->played_item) {
    gst_clear_object (&self->played_item);
    self->played_index = NYX_QUEUE_INVALID_POSITION;
  }
  g_ptr_array_remove_index (self->items, index);

  if (self->running && self->ws_connections->len > 0) {
    _clear_delayed_queue_changed_timeout (self);
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  }
}

static void
nyx_server_queue_item_repositioned (NyxFeature *feature, guint before, guint after)
{
  NyxServer *self = NYX_SERVER_CAST (feature);
  NyxMediaItem *item;

  GST_DEBUG_OBJECT (self, "Queue item repositioned: %u -> %u", before, after);

  item = (NyxMediaItem *) g_ptr_array_steal_index (self->items, before);
  g_ptr_array_insert (self->items, after, item);

  if (self->running && self->ws_connections->len > 0) {
    _clear_delayed_queue_changed_timeout (self);
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  }
}

static void
nyx_server_queue_cleared (NyxFeature *feature)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue cleared");
  _clear_stored_queue (self);

  if (self->running && self->ws_connections->len > 0) {
    _clear_delayed_queue_changed_timeout (self);
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  }
}

static void
nyx_server_queue_progression_changed (NyxFeature *feature,
    NyxQueueProgressionMode mode)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Queue progression changed to: %i", mode);

  if (!self->running || self->ws_connections->len == 0)
    return;

  switch (mode) {
    case NYX_QUEUE_PROGRESSION_NONE:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE (NYX_SERVER_QUEUE_PROGRESSION_NONE));
      break;
    case NYX_QUEUE_PROGRESSION_CONSECUTIVE:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE (NYX_SERVER_QUEUE_PROGRESSION_CONSECUTIVE));
      break;
    case NYX_QUEUE_PROGRESSION_REPEAT_ITEM:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE (NYX_SERVER_QUEUE_PROGRESSION_REPEAT_ITEM));
      break;
    case NYX_QUEUE_PROGRESSION_CAROUSEL:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE (NYX_SERVER_QUEUE_PROGRESSION_CAROUSEL));
      break;
    case NYX_QUEUE_PROGRESSION_SHUFFLE:
      nyx_server_send_ws_message (self,
          NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION_MAKE (NYX_SERVER_QUEUE_PROGRESSION_SHUFFLE));
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static gboolean
nyx_server_prepare (NyxFeature *feature)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Prepare");

  self->server = soup_server_new (
      "server-header", "nyx-server",
      NULL);

  soup_server_add_handler (self->server, "/", (SoupServerCallback) _request_cb, self, NULL);
  soup_server_add_websocket_handler (self->server, "/websocket", NULL, NULL,
      (SoupServerWebsocketCallback) _websocket_connection_cb, self, NULL);

  if (nyx_server_get_enabled (self))
    _start_server (self);

  return TRUE;
}

static gboolean
nyx_server_unprepare (NyxFeature *feature)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Unprepare");

  _stop_server (self);
  _clear_stored_queue (self);

  g_clear_object (&self->server);

  return TRUE;
}

static void
nyx_server_property_changed (NyxFeature *feature, GParamSpec *pspec)
{
  NyxServer *self = NYX_SERVER_CAST (feature);

  GST_DEBUG_OBJECT (self, "Property changed: \"%s\"",
      g_param_spec_get_name (pspec));

  if (pspec == param_specs[PROP_ENABLED]) {
    if (nyx_server_get_enabled (self))
      _start_server (self);
    else
      _stop_server (self);
  } else if (pspec == param_specs[PROP_QUEUE_CONTROLLABLE]) {
    _clear_delayed_queue_changed_timeout (self);
    nyx_server_send_ws_message (self, NYX_SERVER_WS_EVENT_QUEUE_CHANGED);
  }
}

/**
 * nyx_server_new:
 *
 * Creates a new #NyxServer instance.
 *
 * Returns: (transfer full): a new #NyxServer instance.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
NyxServer *
nyx_server_new (void)
{
  NyxServer *server = g_object_new (NYX_TYPE_SERVER, NULL);
  gst_object_ref_sink (server);

  return server;
}

/**
 * nyx_server_set_enabled:
 * @server: a #NyxServer
 * @enabled: if #NyxServer should run
 *
 * Set whether #NyxServer should be running.
 *
 * Note that server feature will run only after being added to the player.
 * It can be however set to enabled earlier. If server was already added,
 * changing this property allows to start/stop server at any time.
 *
 * To be notified when server is actually running/stopped after being enabled/disabled,
 * you can listen for changes to [property@Nyx.Server:running] property.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
void
nyx_server_set_enabled (NyxServer *self, gboolean enabled)
{
  gboolean prev_enabled;

  g_return_if_fail (NYX_IS_SERVER (self));

  prev_enabled = (gboolean) g_atomic_int_exchange (&self->enabled, (gint) enabled);

  if (prev_enabled != enabled)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_ENABLED]);
}

/**
 * nyx_server_get_enabled:
 * @server: a #NyxServer
 *
 * Get whether #NyxServer is set to be running.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
gboolean
nyx_server_get_enabled (NyxServer *self)
{
  g_return_val_if_fail (NYX_IS_SERVER (self), FALSE);

  return (gboolean) g_atomic_int_get (&self->enabled);
}

/**
 * nyx_server_get_running:
 * @server: a #NyxServer
 *
 * Get whether #NyxServer is currently running.
 *
 * Returns: %TRUE if running, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
gboolean
nyx_server_get_running (NyxServer *self)
{
  gboolean running;

  g_return_val_if_fail (NYX_IS_SERVER (self), FALSE);

  GST_OBJECT_LOCK (self);
  running = self->running;
  GST_OBJECT_UNLOCK (self);

  return running;
}

/**
 * nyx_server_set_port:
 * @server: a #NyxServer
 * @port: a port number or 0 for random free port
 *
 * Set server listening port.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
void
nyx_server_set_port (NyxServer *self, guint port)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_SERVER (self));
  g_return_if_fail (port <= PORT_MAX);

  GST_OBJECT_LOCK (self);
  if ((changed = (port != self->port)))
    self->port = port;
  GST_OBJECT_UNLOCK (self);

  if (changed)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_PORT]);
}

/**
 * nyx_server_get_port:
 * @server: a #NyxServer
 *
 * Get requested server listening port.
 *
 * If you want to know the port server is currently listening on,
 * use [method@Nyx.Server.get_current_port] instead.
 *
 * Returns: Requested listening port or 0 when using random port.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
guint
nyx_server_get_port (NyxServer *self)
{
  guint port;

  g_return_val_if_fail (NYX_IS_SERVER (self), 0);

  GST_OBJECT_LOCK (self);
  port = self->port;
  GST_OBJECT_UNLOCK (self);

  return port;
}

/**
 * nyx_server_get_current_port:
 * @server: a #NyxServer
 *
 * Get port on which server is currently listening on.
 *
 * Returns: Current listening port or 0 if server is not listening.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
guint
nyx_server_get_current_port (NyxServer *self)
{
  guint current_port;

  g_return_val_if_fail (NYX_IS_SERVER (self), 0);

  GST_OBJECT_LOCK (self);
  current_port = self->current_port;
  GST_OBJECT_UNLOCK (self);

  return current_port;
}

/**
 * nyx_server_set_queue_controllable:
 * @server: a #NyxServer
 * @controllable: if #NyxQueue should be controllable
 *
 * Set whether remote @server clients can control [class@Nyx.Queue].
 *
 * This includes ability to add/remove items from the queue and selecting
 * current item for playback remotely using WebSocket messages.
 *
 * You probably want to keep this disabled if your application
 * is supposed to manage what is played now and not WebSocket client.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
void
nyx_server_set_queue_controllable (NyxServer *self, gboolean controllable)
{
  gboolean prev_controllable;

  g_return_if_fail (NYX_IS_SERVER (self));

  prev_controllable = (gboolean) g_atomic_int_exchange (&self->queue_controllable, (gint) controllable);

  if (prev_controllable != controllable)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_QUEUE_CONTROLLABLE]);
}

/**
 * nyx_server_get_queue_controllable:
 * @server: a #NyxServer
 *
 * Get whether remote @server clients can control [class@Nyx.Queue].
 *
 * Returns: %TRUE if control over #NyxQueue is allowed, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
 */
gboolean
nyx_server_get_queue_controllable (NyxServer *self)
{
  g_return_val_if_fail (NYX_IS_SERVER (self), FALSE);

  return (gboolean) g_atomic_int_get (&self->queue_controllable);
}

static void
nyx_server_init (NyxServer *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);
  self->ws_connections = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  self->played_index = NYX_QUEUE_INVALID_POSITION;
  self->position_uint = G_MAXUINT;

  g_atomic_int_set (&self->enabled, (gint) DEFAULT_ENABLED);
  g_atomic_int_set (&self->queue_controllable, (gint) DEFAULT_QUEUE_CONTROLLABLE);
}

static void
nyx_server_dispose (GObject *object)
{
  NyxServer *self = NYX_SERVER_CAST (object);

  GST_OBJECT_LOCK (self);

  g_clear_handle_id (&self->error_id, g_source_remove);
  g_clear_handle_id (&self->running_id, g_source_remove);

  GST_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_server_finalize (GObject *object)
{
  NyxServer *self = NYX_SERVER_CAST (object);

  g_ptr_array_unref (self->ws_connections);
  g_ptr_array_unref (self->items);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_server_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxServer *self = NYX_SERVER_CAST (object);

  switch (prop_id) {
    case PROP_ENABLED:
      nyx_server_set_enabled (self, g_value_get_boolean (value));
      break;
    case PROP_PORT:
      nyx_server_set_port (self, g_value_get_uint (value));
      break;
    case PROP_QUEUE_CONTROLLABLE:
      nyx_server_set_queue_controllable (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_server_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxServer *self = NYX_SERVER_CAST (object);

  switch (prop_id) {
    case PROP_ENABLED:
      g_value_set_boolean (value, nyx_server_get_enabled (self));
      break;
    case PROP_RUNNING:
      g_value_set_boolean (value, nyx_server_get_running (self));
      break;
    case PROP_PORT:
      g_value_set_uint (value, nyx_server_get_port (self));
      break;
    case PROP_CURRENT_PORT:
      g_value_set_uint (value, nyx_server_get_current_port (self));
      break;
    case PROP_QUEUE_CONTROLLABLE:
      g_value_set_boolean (value, nyx_server_get_queue_controllable (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_server_class_init (NyxServerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxFeatureClass *feature_class = (NyxFeatureClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxserver", 0,
      "Nyx Server");
  nyx_server_mdns_debug_init ();

  gobject_class->get_property = nyx_server_get_property;
  gobject_class->set_property = nyx_server_set_property;
  gobject_class->dispose = nyx_server_dispose;
  gobject_class->finalize = nyx_server_finalize;

  feature_class->prepare = nyx_server_prepare;
  feature_class->unprepare = nyx_server_unprepare;
  feature_class->property_changed = nyx_server_property_changed;
  feature_class->state_changed = nyx_server_state_changed;
  feature_class->position_changed = nyx_server_position_changed;
  feature_class->speed_changed = nyx_server_speed_changed;
  feature_class->volume_changed = nyx_server_volume_changed;
  feature_class->mute_changed = nyx_server_mute_changed;
  feature_class->played_item_changed = nyx_server_played_item_changed;
  feature_class->item_updated = nyx_server_item_updated;
  feature_class->queue_item_added = nyx_server_queue_item_added;
  feature_class->queue_item_removed = nyx_server_queue_item_removed;
  feature_class->queue_item_repositioned = nyx_server_queue_item_repositioned;
  feature_class->queue_cleared = nyx_server_queue_cleared;
  feature_class->queue_progression_changed = nyx_server_queue_progression_changed;

  /**
   * NyxServer:enabled:
   *
   * Whether server is enabled.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_ENABLED] = g_param_spec_boolean ("enabled",
      NULL, NULL, DEFAULT_ENABLED,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxServer:running:
   *
   * Whether server is currently running.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_RUNNING] = g_param_spec_boolean ("running",
      NULL, NULL, FALSE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxServer:port:
   *
   * Port to listen on or 0 for using random unused port.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_PORT] = g_param_spec_uint ("port",
      NULL, NULL, 0, PORT_MAX, 0,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxServer:current-port:
   *
   * Port on which server is currently listening on or 0 if not listening.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_CURRENT_PORT] = g_param_spec_uint ("current-port",
      NULL, NULL, 0, PORT_MAX, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxServer:queue-controllable:
   *
   * Whether remote server clients can control #NyxQueue.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  param_specs[PROP_QUEUE_CONTROLLABLE] = g_param_spec_boolean ("queue-controllable",
      NULL, NULL, DEFAULT_QUEUE_CONTROLLABLE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxServer::error:
   * @server: a #NyxServer
   * @error: a #GError
   *
   * Error signal when server could not start.
   * This will be emitted from application main thread.
   *
   * Deprecated: 0.10: Use Control Hub from `nyx-enhancers` repo instead.
   */
  signals[SIGNAL_ERROR] = g_signal_new ("error",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
