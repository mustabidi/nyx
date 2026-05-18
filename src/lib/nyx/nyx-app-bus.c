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

#include <gio/gio.h>

#include "nyx-bus-private.h"
#include "nyx-app-bus-private.h"
#include "nyx-player-private.h"
#include "nyx-queue-private.h"
#include "nyx-media-item-private.h"
#include "nyx-timeline-private.h"

#define GST_CAT_DEFAULT nyx_app_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxAppBus
{
  GstBus parent;
};

#define parent_class nyx_app_bus_parent_class
G_DEFINE_TYPE (NyxAppBus, nyx_app_bus, GST_TYPE_BUS);

enum
{
  NYX_APP_BUS_STRUCTURE_UNKNOWN = 0,
  NYX_APP_BUS_STRUCTURE_PROP_NOTIFY,
  NYX_APP_BUS_STRUCTURE_REFRESH_STREAMS,
  NYX_APP_BUS_STRUCTURE_REFRESH_TIMELINE,
  NYX_APP_BUS_STRUCTURE_INSERT_PLAYLIST,
  NYX_APP_BUS_STRUCTURE_SIMPLE_SIGNAL,
  NYX_APP_BUS_STRUCTURE_OBJECT_DESC_SIGNAL,
  NYX_APP_BUS_STRUCTURE_DESC_WITH_DETAILS_SIGNAL,
  NYX_APP_BUS_STRUCTURE_MESSAGE_SIGNAL,
  NYX_APP_BUS_STRUCTURE_ERROR_SIGNAL
};

static NyxBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"prop-notify", 0},
  {"refresh-streams", 0},
  {"refresh-timeline", 0},
  {"insert-playlist", 0},
  {"simple-signal", 0},
  {"object-desc-signal", 0},
  {"desc-with-details-signal", 0},
  {"message", 0},
  {"error-signal", 0},
  {NULL, 0}
};

enum
{
  NYX_APP_BUS_FIELD_UNKNOWN = 0,
  NYX_APP_BUS_FIELD_PSPEC,
  NYX_APP_BUS_FIELD_SIGNAL_ID,
  NYX_APP_BUS_FIELD_OBJECT,
  NYX_APP_BUS_FIELD_OTHER_OBJECT,
  NYX_APP_BUS_FIELD_DESC,
  NYX_APP_BUS_FIELD_DETAILS,
  NYX_APP_BUS_FIELD_ERROR,
  NYX_APP_BUS_FIELD_DEBUG_INFO
};

static NyxBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"pspec", 0},
  {"signal-id", 0},
  {"object", 0},
  {"other-object", 0},
  {"desc", 0},
  {"details", 0},
  {"error", 0},
  {"debug-info", 0},
  {NULL, 0}
};

#define _STRUCTURE_QUARK(q) (_structure_quarks[NYX_APP_BUS_STRUCTURE_##q].quark)
#define _FIELD_QUARK(q) (_field_quarks[NYX_APP_BUS_FIELD_##q].quark)
#define _MESSAGE_SRC_GOBJECT(msg) ((GObject *) GST_MESSAGE_SRC (msg))

void
nyx_app_bus_initialize (void)
{
  gint i;

  for (i = 0; _structure_quarks[i].name; ++i)
    _structure_quarks[i].quark = g_quark_from_static_string (_structure_quarks[i].name);
  for (i = 0; _field_quarks[i].name; ++i)
    _field_quarks[i].quark = g_quark_from_static_string (_field_quarks[i].name);
}

void
nyx_app_bus_forward_message (NyxAppBus *self, GstMessage *msg)
{
  gst_bus_post (GST_BUS_CAST (self), gst_message_ref (msg));
}

/* FIXME: It should be faster to wait for gst_message_new_property_notify() from
 * playbin bus and forward them to app bus instead of connecting to notify
 * signals of playbin, so change into using gst_message_new_property_notify() here too */
void
nyx_app_bus_post_prop_notify (NyxAppBus *self,
    GstObject *src, GParamSpec *pspec)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (PROP_NOTIFY),
      _FIELD_QUARK (PSPEC), G_TYPE_PARAM, pspec,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_prop_notify_msg (GstMessage *msg, const GstStructure *structure)
{
  GParamSpec *pspec = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (PSPEC), G_TYPE_PARAM, &pspec,
      NULL);
  g_object_notify_by_pspec (_MESSAGE_SRC_GOBJECT (msg), pspec);

  g_param_spec_unref (pspec);
}

void
nyx_app_bus_post_refresh_streams (NyxAppBus *self, GstObject *src)
{
  GstStructure *structure = gst_structure_new_id_empty (_STRUCTURE_QUARK (REFRESH_STREAMS));
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_refresh_streams_msg (GstMessage *msg, const GstStructure *structure)
{
  NyxPlayer *player = NYX_PLAYER_CAST (GST_MESSAGE_SRC (msg));
  nyx_player_refresh_streams (player);
}

void
nyx_app_bus_post_refresh_timeline (NyxAppBus *self, GstObject *src)
{
  GstStructure *structure = gst_structure_new_id_empty (_STRUCTURE_QUARK (REFRESH_TIMELINE));
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_refresh_timeline_msg (GstMessage *msg, const GstStructure *structure)
{
  NyxMediaItem *item = NYX_MEDIA_ITEM_CAST (GST_MESSAGE_SRC (msg));
  NyxTimeline *timeline = nyx_media_item_get_timeline (item);

  nyx_timeline_refresh (timeline);
}

void
nyx_app_bus_post_insert_playlist (NyxAppBus *self, GstObject *src,
    GstObject *playlist_item, GObject *playlist)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (INSERT_PLAYLIST),
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, playlist_item,
      _FIELD_QUARK (OTHER_OBJECT), G_TYPE_OBJECT, playlist,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_insert_playlist_msg (GstMessage *msg, const GstStructure *structure)
{
  NyxPlayer *player = NYX_PLAYER_CAST (GST_MESSAGE_SRC (msg));
  NyxQueue *queue = nyx_player_get_queue (player);
  GstObject *playlist_item;
  GObject *playlist;

  gst_structure_id_get (structure,
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, &playlist_item,
      _FIELD_QUARK (OTHER_OBJECT), G_TYPE_OBJECT, &playlist,
      NULL);
  nyx_queue_handle_playlist (queue,
      NYX_MEDIA_ITEM (playlist_item), G_LIST_STORE (playlist));

  gst_object_unref (playlist_item);
  g_object_unref (playlist);
}

void
nyx_app_bus_post_simple_signal (NyxAppBus *self, GstObject *src, guint signal_id)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (SIMPLE_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_simple_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0);
}

void
nyx_app_bus_post_object_desc_signal (NyxAppBus *self,
    GstObject *src, guint signal_id,
    GstObject *object, const gchar *desc)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (OBJECT_DESC_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, object,
      _FIELD_QUARK (DESC), G_TYPE_STRING, desc,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_object_desc_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  GstObject *object = NULL;
  gchar *desc = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (OBJECT), GST_TYPE_OBJECT, &object,
      _FIELD_QUARK (DESC), G_TYPE_STRING, &desc,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, object, desc);

  gst_object_unref (object);
  g_free (desc);
}

void
nyx_app_bus_post_desc_with_details_signal (NyxAppBus *self,
    GstObject *src, guint signal_id,
    const gchar *desc, const gchar *details)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (DESC_WITH_DETAILS_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (DESC), G_TYPE_STRING, desc,
      _FIELD_QUARK (DETAILS), G_TYPE_STRING, details,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_desc_with_details_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  gchar *desc = NULL, *details = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (DESC), G_TYPE_STRING, &desc,
      _FIELD_QUARK (DETAILS), G_TYPE_STRING, &details,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, desc, details);

  g_free (desc);
  g_free (details);
}

void
nyx_app_bus_post_message_signal (NyxAppBus *self,
    GstObject *src, guint signal_id, GstMessage *msg)
{
  /* Check for any "message" signal connection */
  if (g_signal_handler_find (src, G_SIGNAL_MATCH_ID,
      signal_id, 0, NULL, NULL, NULL) != 0) {
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark detail;

    if (G_UNLIKELY (structure == NULL))
      return;

    detail = g_quark_from_string (gst_structure_get_name (structure));

    /* If specific detail is connected or ALL "message" handler */
    if (g_signal_handler_find (src, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL,
        signal_id, detail, NULL, NULL, NULL) != 0
        || g_signal_handler_find (src, G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL,
        signal_id, 0, NULL, NULL, NULL) != 0) {
      GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (MESSAGE_SIGNAL),
          _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
          _FIELD_QUARK (DETAILS), G_TYPE_UINT, detail,
          _FIELD_QUARK (OBJECT), GST_TYPE_MESSAGE, msg,
          NULL);
      gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
    }
  }
}

static inline void
_handle_message_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  GQuark detail = 0;
  GstMessage *fwd_message = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (DETAILS), G_TYPE_UINT, &detail,
      _FIELD_QUARK (OBJECT), GST_TYPE_MESSAGE, &fwd_message,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, detail, fwd_message);

  gst_message_unref (fwd_message);
}

void
nyx_app_bus_post_error_signal (NyxAppBus *self,
    GstObject *src, guint signal_id,
    GError *error, const gchar *debug_info)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (ERROR_SIGNAL),
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, signal_id,
      _FIELD_QUARK (ERROR), G_TYPE_ERROR, error,
      _FIELD_QUARK (DEBUG_INFO), G_TYPE_STRING, debug_info,
      NULL);
  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (src, structure));
}

static inline void
_handle_error_signal_msg (GstMessage *msg, const GstStructure *structure)
{
  guint signal_id = 0;
  GError *error = NULL;
  gchar *debug_info = NULL;

  gst_structure_id_get (structure,
      _FIELD_QUARK (SIGNAL_ID), G_TYPE_UINT, &signal_id,
      _FIELD_QUARK (ERROR), G_TYPE_ERROR, &error,
      _FIELD_QUARK (DEBUG_INFO), G_TYPE_STRING, &debug_info,
      NULL);
  g_signal_emit (_MESSAGE_SRC_GOBJECT (msg), signal_id, 0, error, debug_info);

  g_clear_error (&error);
  g_free (debug_info);
}

static gboolean
nyx_app_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _STRUCTURE_QUARK (PROP_NOTIFY))
      _handle_prop_notify_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (REFRESH_STREAMS))
      _handle_refresh_streams_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (REFRESH_TIMELINE))
      _handle_refresh_timeline_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (INSERT_PLAYLIST))
      _handle_insert_playlist_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (SIMPLE_SIGNAL))
      _handle_simple_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (OBJECT_DESC_SIGNAL))
      _handle_object_desc_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (MESSAGE_SIGNAL))
      _handle_message_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (ERROR_SIGNAL))
      _handle_error_signal_msg (msg, structure);
    else if (quark == _STRUCTURE_QUARK (DESC_WITH_DETAILS_SIGNAL))
      _handle_desc_with_details_signal_msg (msg, structure);
  }

  return G_SOURCE_CONTINUE;
}

/*
 * nyx_app_bus_new:
 *
 * Returns: (transfer full): a new #NyxAppBus instance
 */
NyxAppBus *
nyx_app_bus_new (void)
{
  GstBus *app_bus;

  app_bus = GST_BUS_CAST (g_object_new (NYX_TYPE_APP_BUS, NULL));
  gst_object_ref_sink (app_bus);

  gst_bus_add_watch (app_bus, (GstBusFunc) nyx_app_bus_message_func, NULL);

  return NYX_APP_BUS_CAST (app_bus);
}

static void
nyx_app_bus_init (NyxAppBus *self)
{
}

static void
nyx_app_bus_finalize (GObject *object)
{
  NyxAppBus *self = NYX_APP_BUS_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_app_bus_class_init (NyxAppBusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxappbus", 0,
      "Nyx App Bus");

  gobject_class->finalize = nyx_app_bus_finalize;
}
