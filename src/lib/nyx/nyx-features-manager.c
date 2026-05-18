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

#include "nyx-features-manager-private.h"
#include "nyx-features-bus-private.h"
#include "nyx-feature-private.h"

#define GST_CAT_DEFAULT nyx_features_manager_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxFeaturesManager
{
  NyxThreadedObject parent;

  GPtrArray *features;
  NyxFeaturesBus *bus;
};

#define parent_class nyx_features_manager_parent_class
G_DEFINE_TYPE (NyxFeaturesManager, nyx_features_manager, NYX_TYPE_THREADED_OBJECT);

static inline void
_post_object (NyxFeaturesManager *self, NyxFeaturesManagerEvent event, GObject *data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, data);

  nyx_features_bus_post_event (self->bus, self, event, &value, NULL);
}

static inline void
_post_int (NyxFeaturesManager *self, NyxFeaturesManagerEvent event, gint data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, data);

  nyx_features_bus_post_event (self->bus, self, event, &value, NULL);
}

static inline void
_post_double (NyxFeaturesManager *self, NyxFeaturesManagerEvent event, gdouble data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_DOUBLE);
  g_value_set_double (&value, data);

  nyx_features_bus_post_event (self->bus, self, event, &value, NULL);
}

static inline void
_post_boolean (NyxFeaturesManager *self, NyxFeaturesManagerEvent event, gboolean data)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, data);

  nyx_features_bus_post_event (self->bus, self, event, &value, NULL);
}

static inline void
_post_item_added_or_removed (NyxFeaturesManager *self, NyxFeaturesManagerEvent event,
    NyxMediaItem *item, guint index)
{
  GValue value = G_VALUE_INIT;
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, (GObject *) item);

  g_value_init (&extra_value, G_TYPE_UINT);
  g_value_set_uint (&extra_value, index);

  nyx_features_bus_post_event (self->bus, self, event, &value, &extra_value);
}

static inline void
_post_item_reposition (NyxFeaturesManager *self, guint data_1, guint data_2)
{
  GValue value = G_VALUE_INIT;
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, data_1);

  g_value_init (&extra_value, G_TYPE_UINT);
  g_value_set_uint (&extra_value, data_2);

  nyx_features_bus_post_event (self->bus, self,
      NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED, &value, &extra_value);
}

/*
 * nyx_features_manager_new:
 *
 * Returns: (transfer full): a new #NyxFeaturesManager instance.
 */
NyxFeaturesManager *
nyx_features_manager_new (void)
{
  NyxFeaturesManager *features_manager;

  features_manager = g_object_new (NYX_TYPE_FEATURES_MANAGER, NULL);
  gst_object_ref_sink (features_manager);

  return features_manager;
}

void
nyx_features_manager_add_feature (NyxFeaturesManager *self, NyxFeature *feature, GstObject *parent)
{
  GValue value = G_VALUE_INIT;
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, G_OBJECT (feature));

  g_value_init (&extra_value, G_TYPE_OBJECT);
  g_value_set_object (&extra_value, G_OBJECT (parent));

  nyx_features_bus_post_event (self->bus, self,
      NYX_FEATURES_MANAGER_EVENT_FEATURE_ADDED, &value, &extra_value);
}

void
nyx_features_manager_trigger_property_changed (NyxFeaturesManager *self, NyxFeature *feature, GParamSpec *pspec)
{
  GValue value = G_VALUE_INIT;
  GValue extra_value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);
  g_value_set_object (&value, G_OBJECT (feature));

  g_value_init (&extra_value, G_TYPE_PARAM);
  g_value_set_param (&extra_value, pspec);

  nyx_features_bus_post_event (self->bus, self,
      NYX_FEATURES_MANAGER_EVENT_FEATURE_PROPERTY_CHANGED, &value, &extra_value);
}

void
nyx_features_manager_trigger_state_changed (NyxFeaturesManager *self, NyxPlayerState state)
{
  _post_int (self, NYX_FEATURES_MANAGER_EVENT_STATE_CHANGED, state);
}

void
nyx_features_manager_trigger_position_changed (NyxFeaturesManager *self, gdouble position)
{
  _post_double (self, NYX_FEATURES_MANAGER_EVENT_POSITION_CHANGED, position);
}

void
nyx_features_manager_trigger_speed_changed (NyxFeaturesManager *self, gdouble speed)
{
  _post_double (self, NYX_FEATURES_MANAGER_EVENT_SPEED_CHANGED, speed);
}

void
nyx_features_manager_trigger_volume_changed (NyxFeaturesManager *self, gdouble volume)
{
  _post_double (self, NYX_FEATURES_MANAGER_EVENT_VOLUME_CHANGED, volume);
}

void
nyx_features_manager_trigger_mute_changed (NyxFeaturesManager *self, gboolean mute)
{
  _post_boolean (self, NYX_FEATURES_MANAGER_EVENT_MUTE_CHANGED, mute);
}

void
nyx_features_manager_trigger_played_item_changed (NyxFeaturesManager *self, NyxMediaItem *item)
{
  _post_object (self, NYX_FEATURES_MANAGER_EVENT_PLAYED_ITEM_CHANGED, (GObject *) item);
}

void
nyx_features_manager_trigger_item_updated (NyxFeaturesManager *self, NyxMediaItem *item)
{
  _post_object (self, NYX_FEATURES_MANAGER_EVENT_ITEM_UPDATED, (GObject *) item);
}

void
nyx_features_manager_trigger_queue_item_added (NyxFeaturesManager *self, NyxMediaItem *item, guint index)
{
  _post_item_added_or_removed (self, NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED, item, index);
}

void
nyx_features_manager_trigger_queue_item_removed (NyxFeaturesManager *self, NyxMediaItem *item, guint index)
{
  _post_item_added_or_removed (self, NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED, item, index);
}

void
nyx_features_manager_trigger_queue_item_repositioned (NyxFeaturesManager *self, guint before, guint after)
{
  _post_item_reposition (self, before, after);
}

void
nyx_features_manager_trigger_queue_cleared (NyxFeaturesManager *self)
{
  nyx_features_bus_post_event (self->bus, self, NYX_FEATURES_MANAGER_EVENT_QUEUE_CLEARED, NULL, NULL);
}

void
nyx_features_manager_trigger_queue_progression_changed (NyxFeaturesManager *self, NyxQueueProgressionMode mode)
{
  _post_int (self, NYX_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED, mode);
}

void
nyx_features_manager_handle_event (NyxFeaturesManager *self, NyxFeaturesManagerEvent event,
    const GValue *value, const GValue *extra_value)
{
  guint i;

  switch (event) {
    case NYX_FEATURES_MANAGER_EVENT_FEATURE_ADDED:{
      NyxFeature *feature = g_value_get_object (value);
      GstObject *parent = g_value_get_object (extra_value);

      if (!g_ptr_array_find (self->features, feature, NULL)) {
        g_ptr_array_add (self->features, gst_object_ref (feature));
        gst_object_set_parent (GST_OBJECT_CAST (feature), parent);

        nyx_feature_call_prepare (feature);
      }

      /* Nothing more to do */
      return;
    }
    default:
      break;
  }

  for (i = 0; i < self->features->len; ++i) {
    NyxFeature *feature = g_ptr_array_index (self->features, i);

    switch (event) {
      case NYX_FEATURES_MANAGER_EVENT_FEATURE_PROPERTY_CHANGED:{
        NyxFeature *event_feature = g_value_get_object (value);

        if (feature == event_feature) {
          nyx_feature_call_property_changed (feature,
              g_value_get_param (extra_value));
        }
        break;
      }
      case NYX_FEATURES_MANAGER_EVENT_STATE_CHANGED:
        nyx_feature_call_state_changed (feature, g_value_get_int (value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_POSITION_CHANGED:
        nyx_feature_call_position_changed (feature, g_value_get_double (value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_SPEED_CHANGED:
        nyx_feature_call_speed_changed (feature, g_value_get_double (value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_VOLUME_CHANGED:
        nyx_feature_call_volume_changed (feature, g_value_get_double (value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_MUTE_CHANGED:
        nyx_feature_call_mute_changed (feature, g_value_get_boolean (value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_PLAYED_ITEM_CHANGED:
        nyx_feature_call_played_item_changed (feature,
            NYX_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case NYX_FEATURES_MANAGER_EVENT_ITEM_UPDATED:
        nyx_feature_call_item_updated (feature,
            NYX_MEDIA_ITEM_CAST (g_value_get_object (value)));
        break;
      case NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED:
        nyx_feature_call_queue_item_added (feature,
            NYX_MEDIA_ITEM_CAST (g_value_get_object (value)),
            g_value_get_uint (extra_value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED:
        nyx_feature_call_queue_item_removed (feature,
            NYX_MEDIA_ITEM_CAST (g_value_get_object (value)),
            g_value_get_uint (extra_value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED:
        nyx_feature_call_queue_item_repositioned (feature,
            g_value_get_uint (value),
            g_value_get_uint (extra_value));
        break;
      case NYX_FEATURES_MANAGER_EVENT_QUEUE_CLEARED:
        nyx_feature_call_queue_cleared (feature);
        break;
      case NYX_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED:
        nyx_feature_call_queue_progression_changed (feature, g_value_get_int (value));
        break;
      default:
        break;
    }
  }
}

static void
nyx_features_manager_thread_start (NyxThreadedObject *threaded_object)
{
  NyxFeaturesManager *self = NYX_FEATURES_MANAGER_CAST (threaded_object);

  GST_TRACE_OBJECT (threaded_object, "Features manager thread start");

  self->features = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_object_unref);
  self->bus = nyx_features_bus_new ();
}

static void
nyx_features_manager_thread_stop (NyxThreadedObject *threaded_object)
{
  NyxFeaturesManager *self = NYX_FEATURES_MANAGER_CAST (threaded_object);
  guint i;

  GST_TRACE_OBJECT (threaded_object, "Features manager thread stop");

  gst_bus_set_flushing (GST_BUS_CAST (self->bus), TRUE);
  gst_bus_remove_watch (GST_BUS_CAST (self->bus));
  gst_clear_object (&self->bus);

  for (i = 0; i < self->features->len; ++i) {
    NyxFeature *feature = g_ptr_array_index (self->features, i);

    nyx_feature_call_unprepare (feature);
    gst_object_unparent (GST_OBJECT_CAST (feature));
  }

  g_ptr_array_unref (self->features);
}

static void
nyx_features_manager_init (NyxFeaturesManager *self)
{
}

static void
nyx_features_manager_class_init (NyxFeaturesManagerClass *klass)
{
  NyxThreadedObjectClass *threaded_object = (NyxThreadedObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxfeaturesmanager", 0,
      "Nyx Features Manager");

  threaded_object->thread_start = nyx_features_manager_thread_start;
  threaded_object->thread_stop = nyx_features_manager_thread_stop;
}
