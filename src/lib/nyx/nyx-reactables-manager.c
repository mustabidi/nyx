/* Nyx Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <gst/gst.h>

#include "nyx-reactables-manager-private.h"
#include "nyx-reactable.h"
#include "nyx-bus-private.h"
#include "nyx-player.h"
#include "nyx-enhancer-proxy-list.h"
#include "nyx-enhancer-proxy-private.h"
#include "nyx-utils-private.h"

#include "nyx-functionalities-availability.h"

#if NYX_WITH_ENHANCERS_LOADER
#include "nyx-enhancers-loader-private.h"
#endif

#define CONFIG_STRUCTURE_NAME "config"

#define GST_CAT_DEFAULT nyx_reactables_manager_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxReactablesManager
{
  NyxThreadedObject parent;

  GstBus *bus;
  GPtrArray *array;

  gboolean prepare_called;
};

#define parent_class nyx_reactables_manager_parent_class
G_DEFINE_TYPE (NyxReactablesManager, nyx_reactables_manager, NYX_TYPE_THREADED_OBJECT);

typedef struct
{
  NyxReactable *reactable;
  NyxEnhancerProxy *proxy;
  GSettings *settings;
} NyxReactableManagerData;

enum
{
  NYX_REACTABLES_MANAGER_EVENT_INVALID = 0,
  NYX_REACTABLES_MANAGER_EVENT_STATE_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_POSITION_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_SPEED_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_VOLUME_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_MUTE_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_PLAYED_ITEM_CHANGED,
  NYX_REACTABLES_MANAGER_EVENT_ITEM_UPDATED,
  NYX_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_ADDED,
  NYX_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_REMOVED,
  NYX_REACTABLES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED,
  NYX_REACTABLES_MANAGER_EVENT_QUEUE_CLEARED,
  NYX_REACTABLES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED
};

enum
{
  NYX_REACTABLES_MANAGER_QUARK_CONFIGURE = 0,
  NYX_REACTABLES_MANAGER_QUARK_EVENT,
  NYX_REACTABLES_MANAGER_QUARK_USER_MESSAGE,
  NYX_REACTABLES_MANAGER_QUARK_VALUE,
  NYX_REACTABLES_MANAGER_QUARK_EXTRA_VALUE
};

static NyxBusQuark _quarks[] = {
  {"configure", 0},
  {"event", 0},
  {"user-message", 0},
  {"value", 0},
  {"extra-value", 0},
  {NULL, 0}
};

#define _EVENT(e) G_PASTE(NYX_REACTABLES_MANAGER_EVENT_, e)
#define _QUARK(q) (_quarks[NYX_REACTABLES_MANAGER_QUARK_##q].quark)

#define _BUS_POST_EVENT_SINGLE(event_id,lower,type,val) { \
  GValue _value = G_VALUE_INIT;                           \
  g_value_init (&_value, type);                           \
  g_value_set_##lower (&_value, val);                     \
  _bus_post_event (self, event_id, &_value, NULL); }

#define _BUS_POST_EVENT_DUAL(event_id,lower1,type1,val1,lower2,type2,val2) { \
  GValue _value1 = G_VALUE_INIT;                                             \
  GValue _value2 = G_VALUE_INIT;                                             \
  g_value_init (&_value1, type1);                                            \
  g_value_init (&_value2, type2);                                            \
  g_value_set_##lower1 (&_value1, val1);                                     \
  g_value_set_##lower2 (&_value2, val2);                                     \
  _bus_post_event (self, event_id, &_value1, &_value2); }

void
nyx_reactables_manager_initialize (void)
{
  gint i;

  for (i = 0; _quarks[i].name; ++i)
    _quarks[i].quark = g_quark_from_static_string (_quarks[i].name);
}

static void
_settings_changed_cb (GSettings *settings, const gchar *key, NyxReactableManagerData *data)
{
  GST_DEBUG_OBJECT (data->reactable, "Global setting \"%s\" changed", key);

  /* Local settings are applied through bus events, so all that is
   * needed here is a check to not overwrite locally set setting */
  if (!nyx_enhancer_proxy_has_locally_set (data->proxy, key)) {
    GVariant *variant = g_settings_get_value (settings, key);
    GValue value = G_VALUE_INIT;

    if (G_LIKELY (nyx_utils_set_value_from_variant (&value, variant))) {
      g_object_set_property (G_OBJECT (data->reactable), key, &value);
      g_value_unset (&value);
    }

    g_variant_unref (variant);
  }
}

static inline void
nyx_reactables_manager_handle_prepare (NyxReactablesManager *self)
{
  NyxPlayer *player;

  GST_INFO_OBJECT (self, "Preparing reactable enhancers");
  player = NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));

  if (G_LIKELY (player != NULL)) {
    NyxEnhancerProxyList *proxies = nyx_player_get_enhancer_proxies (player);
    guint i, n_proxies = nyx_enhancer_proxy_list_get_n_proxies (proxies);

    for (i = 0; i < n_proxies; ++i) {
      NyxEnhancerProxy *proxy = nyx_enhancer_proxy_list_peek_proxy (proxies, i);
      NyxReactable *reactable = NULL;

      if (!nyx_enhancer_proxy_target_has_interface (proxy, NYX_TYPE_REACTABLE))
        continue;

#if NYX_WITH_ENHANCERS_LOADER
      reactable = NYX_REACTABLE_CAST (
          nyx_enhancers_loader_create_enhancer (proxy, NYX_TYPE_REACTABLE));
#endif

      if (reactable) {
        NyxReactableManagerData *data;
        GstStructure *config;

        if (g_object_is_floating (reactable))
          gst_object_ref_sink (reactable);

        data = g_new (NyxReactableManagerData, 1);
        data->reactable = reactable;
        data->proxy = gst_object_ref (proxy);
        data->settings = nyx_enhancer_proxy_get_settings (proxy);

        GST_TRACE_OBJECT (self, "Created data for reactable: %" GST_PTR_FORMAT, data->reactable);

        /* Settings are stored in data in order for this signal to keep working */
        if (data->settings)
          g_signal_connect (data->settings, "changed", G_CALLBACK (_settings_changed_cb), data);

        if ((config = nyx_enhancer_proxy_make_current_config (proxy))) {
          nyx_enhancer_proxy_apply_config_to_enhancer (proxy, config, (GObject *) reactable);
          gst_structure_free (config);
        }

        g_ptr_array_add (self->array, data);
        gst_object_set_parent (GST_OBJECT_CAST (data->reactable), GST_OBJECT_CAST (player));
      }
    }

    GST_INFO_OBJECT (self, "Prepared %i reactable enhancers", self->array->len);
    gst_object_unref (player);
  } else {
    GST_ERROR_OBJECT (self, "Could not prepare reactable enhancers!");
  }
}

static inline void
nyx_reactables_manager_handle_configure (NyxReactablesManager *self, const GstStructure *structure)
{
  const GValue *proxy_val, *config_val;
  NyxEnhancerProxy *proxy;
  const GstStructure *config;
  guint i;

  proxy_val = gst_structure_id_get_value (structure, _QUARK (VALUE));
  config_val = gst_structure_id_get_value (structure, _QUARK (EXTRA_VALUE));

  proxy = NYX_ENHANCER_PROXY_CAST (g_value_get_object (proxy_val));
  config = gst_value_get_structure (config_val);

  for (i = 0; i < self->array->len; ++i) {
    NyxReactableManagerData *data = g_ptr_array_index (self->array, i);

    if (data->proxy == proxy) {
      nyx_enhancer_proxy_apply_config_to_enhancer (data->proxy,
          config, (GObject *) data->reactable);
      break;
    }
  }
}

static inline void
nyx_reactables_manager_handle_event (NyxReactablesManager *self, const GstStructure *structure)
{
  const GValue *value = gst_structure_id_get_value (structure, _QUARK (VALUE));
  const GValue *extra_value = gst_structure_id_get_value (structure, _QUARK (EXTRA_VALUE));
  guint i, event_id;

  if (G_UNLIKELY (!gst_structure_id_get (structure,
      _QUARK (EVENT), G_TYPE_ENUM, &event_id, NULL))) {
    GST_ERROR_OBJECT (self, "Could not read event ID");
    return;
  }

  for (i = 0; i < self->array->len; ++i) {
    NyxReactableManagerData *data = g_ptr_array_index (self->array, i);
    NyxReactableInterface *reactable_iface = NYX_REACTABLE_GET_IFACE (data->reactable);

    switch (event_id) {
      case _EVENT (STATE_CHANGED):
        if (reactable_iface->state_changed)
          reactable_iface->state_changed (data->reactable, g_value_get_int (value));
        break;
      case _EVENT (POSITION_CHANGED):
        if (reactable_iface->position_changed)
          reactable_iface->position_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (SPEED_CHANGED):
        if (reactable_iface->speed_changed)
          reactable_iface->speed_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (VOLUME_CHANGED):
        if (reactable_iface->volume_changed)
          reactable_iface->volume_changed (data->reactable, g_value_get_double (value));
        break;
      case _EVENT (MUTE_CHANGED):
        if (reactable_iface->mute_changed)
          reactable_iface->mute_changed (data->reactable, g_value_get_boolean (value));
        break;
      case _EVENT (PLAYED_ITEM_CHANGED):
        if (reactable_iface->played_item_changed) {
          reactable_iface->played_item_changed (data->reactable,
              NYX_MEDIA_ITEM_CAST (g_value_get_object (value)));
        }
        break;
      case _EVENT (ITEM_UPDATED):
        if (reactable_iface->item_updated) {
          reactable_iface->item_updated (data->reactable,
              NYX_MEDIA_ITEM_CAST (g_value_get_object (value)),
              g_value_get_flags (extra_value));
        }
        break;
      case _EVENT (QUEUE_ITEM_ADDED):
        if (reactable_iface->queue_item_added) {
          reactable_iface->queue_item_added (data->reactable,
              NYX_MEDIA_ITEM_CAST (g_value_get_object (value)),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_ITEM_REMOVED):
        if (reactable_iface->queue_item_removed) {
          reactable_iface->queue_item_removed (data->reactable,
              NYX_MEDIA_ITEM_CAST (g_value_get_object (value)),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_ITEM_REPOSITIONED):
        if (reactable_iface->queue_item_repositioned) {
          reactable_iface->queue_item_repositioned (data->reactable,
              g_value_get_uint (value),
              g_value_get_uint (extra_value));
        }
        break;
      case _EVENT (QUEUE_CLEARED):
        if (reactable_iface->queue_cleared)
          reactable_iface->queue_cleared (data->reactable);
        break;
      case _EVENT (QUEUE_PROGRESSION_CHANGED):
        if (reactable_iface->queue_progression_changed)
          reactable_iface->queue_progression_changed (data->reactable, g_value_get_int (value));
        break;
      default:
        GST_ERROR_OBJECT (self, "Invalid event ID on reactables bus: %u", event_id);
        break;
    }
  }
}

static inline void
nyx_reactables_manager_handle_user_message (NyxReactablesManager *self, const GstStructure *structure)
{
  const GValue *value = gst_structure_id_get_value (structure, _QUARK (VALUE));
  guint i;

  for (i = 0; i < self->array->len; ++i) {
    NyxReactableManagerData *data = g_ptr_array_index (self->array, i);
    NyxReactableInterface *reactable_iface = NYX_REACTABLE_GET_IFACE (data->reactable);

    if (reactable_iface->message_received) {
      reactable_iface->message_received (data->reactable,
          GST_MESSAGE_CAST (g_value_get_boxed (value)));
    }
  }
}

static gboolean
_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    NyxReactablesManager *self = NYX_REACTABLES_MANAGER_CAST (GST_MESSAGE_SRC (msg));
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _QUARK (EVENT)) {
      if (G_UNLIKELY (!self->prepare_called)) {
        nyx_reactables_manager_handle_prepare (self);
        self->prepare_called = TRUE;
      }
      nyx_reactables_manager_handle_event (self, structure);
    } else if (quark == _QUARK (CONFIGURE)) {
      nyx_reactables_manager_handle_configure (self, structure);
    } else if (quark == _QUARK (USER_MESSAGE)) {
      nyx_reactables_manager_handle_user_message (self, structure);
    } else {
      GST_ERROR_OBJECT (self, "Received invalid quark on reactables bus!");
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
_bus_post_event (NyxReactablesManager *self, guint event_id,
    GValue *value, GValue *extra_value)
{
  GstStructure *structure = gst_structure_new_id (_QUARK (EVENT),
      _QUARK (EVENT), G_TYPE_ENUM, event_id,
      NULL);

  if (value)
    gst_structure_id_take_value (structure, _QUARK (VALUE), value);
  if (extra_value)
    gst_structure_id_take_value (structure, _QUARK (EXTRA_VALUE), extra_value);

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

/*
 * nyx_reactables_manager_new:
 *
 * Returns: (transfer full): a new #NyxReactablesManager instance.
 */
NyxReactablesManager *
nyx_reactables_manager_new (void)
{
  NyxReactablesManager *reactables_manager;

  reactables_manager = g_object_new (NYX_TYPE_REACTABLES_MANAGER, NULL);
  gst_object_ref_sink (reactables_manager);

  return reactables_manager;
}

void
nyx_reactables_manager_post_message (NyxReactablesManager *self, GstMessage *msg)
{
  GstStructure *structure = gst_structure_new_id_empty (_QUARK (USER_MESSAGE));
  GValue value = G_VALUE_INIT;

  g_value_init (&value, GST_TYPE_MESSAGE);
  g_value_take_boxed (&value, msg);

  gst_structure_id_take_value (structure, _QUARK (VALUE), &value);

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

void
nyx_reactables_manager_trigger_configure_take_config (NyxReactablesManager *self,
    NyxEnhancerProxy *proxy, GstStructure *config)
{
  GstStructure *structure = gst_structure_new_id (_QUARK (CONFIGURE),
      _QUARK (VALUE), G_TYPE_OBJECT, proxy, NULL);
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&extra_value, GST_TYPE_STRUCTURE);
  g_value_take_boxed (&extra_value, config);

  gst_structure_id_take_value (structure, _QUARK (EXTRA_VALUE), &extra_value);

  gst_bus_post (self->bus, gst_message_new_application (
      GST_OBJECT_CAST (self), structure));
}

void
nyx_reactables_manager_trigger_state_changed (NyxReactablesManager *self, NyxPlayerState state)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (STATE_CHANGED), int, G_TYPE_INT, state);
}

void
nyx_reactables_manager_trigger_position_changed (NyxReactablesManager *self, gdouble position)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (POSITION_CHANGED), double, G_TYPE_DOUBLE, position);
}

void
nyx_reactables_manager_trigger_speed_changed (NyxReactablesManager *self, gdouble speed)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (SPEED_CHANGED), double, G_TYPE_DOUBLE, speed);
}

void
nyx_reactables_manager_trigger_volume_changed (NyxReactablesManager *self, gdouble volume)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (VOLUME_CHANGED), double, G_TYPE_DOUBLE, volume);
}

void
nyx_reactables_manager_trigger_mute_changed (NyxReactablesManager *self, gboolean mute)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (MUTE_CHANGED), boolean, G_TYPE_BOOLEAN, mute);
}

void
nyx_reactables_manager_trigger_played_item_changed (NyxReactablesManager *self, NyxMediaItem *item)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (PLAYED_ITEM_CHANGED), object, NYX_TYPE_MEDIA_ITEM, item);
}

void
nyx_reactables_manager_trigger_item_updated (NyxReactablesManager *self, NyxMediaItem *item, NyxReactableItemUpdatedFlags _flags)
{
  _BUS_POST_EVENT_DUAL (_EVENT (ITEM_UPDATED), object, NYX_TYPE_MEDIA_ITEM, item, flags, NYX_TYPE_REACTABLE_ITEM_UPDATED_FLAGS, _flags);
}

void
nyx_reactables_manager_trigger_queue_item_added (NyxReactablesManager *self, NyxMediaItem *item, guint index)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_ADDED), object, NYX_TYPE_MEDIA_ITEM, item, uint, G_TYPE_UINT, index);
}

void
nyx_reactables_manager_trigger_queue_item_removed (NyxReactablesManager *self, NyxMediaItem *item, guint index)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_REMOVED), object, NYX_TYPE_MEDIA_ITEM, item, uint, G_TYPE_UINT, index);
}

void
nyx_reactables_manager_trigger_queue_item_repositioned (NyxReactablesManager *self, guint before, guint after)
{
  _BUS_POST_EVENT_DUAL (_EVENT (QUEUE_ITEM_REPOSITIONED), uint, G_TYPE_UINT, before, uint, G_TYPE_UINT, after);
}

void
nyx_reactables_manager_trigger_queue_cleared (NyxReactablesManager *self)
{
  _bus_post_event (self, _EVENT (QUEUE_CLEARED), NULL, NULL);
}

void
nyx_reactables_manager_trigger_queue_progression_changed (NyxReactablesManager *self, NyxQueueProgressionMode mode)
{
  _BUS_POST_EVENT_SINGLE (_EVENT (QUEUE_PROGRESSION_CHANGED), int, G_TYPE_INT, mode);
}

static void
_data_remove_func (NyxReactableManagerData *data)
{
  GST_TRACE ("Removing data for reactable: %" GST_PTR_FORMAT, data->reactable);

  g_clear_object (&data->settings);

  gst_object_unparent (GST_OBJECT_CAST (data->reactable));
  gst_object_unref (data->reactable);

  gst_object_unref (data->proxy);
  g_free (data);
}

static void
nyx_reactables_manager_thread_start (NyxThreadedObject *threaded_object)
{
  NyxReactablesManager *self = NYX_REACTABLES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Reactables manager thread start");

  self->array = g_ptr_array_new_with_free_func (
      (GDestroyNotify) _data_remove_func);

  self->bus = gst_bus_new ();
  gst_bus_add_watch (self->bus, (GstBusFunc) _bus_message_func, NULL);
}

static void
nyx_reactables_manager_thread_stop (NyxThreadedObject *threaded_object)
{
  NyxReactablesManager *self = NYX_REACTABLES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (self, "Reactables manager thread stop");

  gst_bus_set_flushing (self->bus, TRUE);
  gst_bus_remove_watch (self->bus);
  gst_clear_object (&self->bus);

  g_ptr_array_unref (self->array);
}

static void
nyx_reactables_manager_init (NyxReactablesManager *self)
{
}

static void
nyx_reactables_manager_finalize (GObject *object)
{
  GST_TRACE_OBJECT (object, "Finalize");
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_reactables_manager_class_init (NyxReactablesManagerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxThreadedObjectClass *threaded_object = (NyxThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxreactablesmanager", 0,
      "Nyx Reactables Manager");

  gobject_class->finalize = nyx_reactables_manager_finalize;

  threaded_object->thread_start = nyx_reactables_manager_thread_start;
  threaded_object->thread_stop = nyx_reactables_manager_thread_stop;
}
