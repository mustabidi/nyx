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
 * NyxFeature:
 *
 * A base class for creating new features for the player.
 *
 * Feature objects are meant for adding additional functionalities that
 * are supposed to either act on playback/properties changes and/or change
 * them themselves due to some external signal/event.
 *
 * For reacting to playback changes subclass should override this class
 * virtual functions logic, while for controlling playback implementation
 * may call [method@Gst.Object.get_parent] to acquire a weak reference on
 * a parent [class@Nyx.Player] object feature was added to.
 *
 * Deprecated: 0.10: Use [iface@Nyx.Reactable] instead.
 */

#include "nyx-feature.h"
#include "nyx-feature-private.h"
#include "nyx-player-private.h"

#define GST_CAT_DEFAULT nyx_feature_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _NyxFeaturePrivate NyxFeaturePrivate;

struct _NyxFeaturePrivate
{
  gboolean prepared;
};

#define parent_class nyx_feature_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (NyxFeature, nyx_feature, GST_TYPE_OBJECT);

#define CALL_WITH_ARGS(_feature,_vfunc,...)                                      \
  NyxFeaturePrivate *priv = nyx_feature_get_instance_private (_feature); \
  if (priv->prepared) {                                                          \
    NyxFeatureClass *feature_class = NYX_FEATURE_GET_CLASS (_feature);   \
    if (feature_class->_vfunc)                                                   \
      feature_class->_vfunc (_feature, __VA_ARGS__); }

#define CALL_WITHOUT_ARGS(_feature,_vfunc)                                       \
  NyxFeaturePrivate *priv = nyx_feature_get_instance_private (_feature); \
  if (priv->prepared) {                                                          \
    NyxFeatureClass *feature_class = NYX_FEATURE_GET_CLASS (_feature);   \
    if (feature_class->_vfunc)                                                   \
      feature_class->_vfunc (_feature); }

void
nyx_feature_call_prepare (NyxFeature *self)
{
  NyxFeaturePrivate *priv = nyx_feature_get_instance_private (self);

  if (!priv->prepared) {
    NyxFeatureClass *feature_class = NYX_FEATURE_GET_CLASS (self);
    gboolean prepared = TRUE; // mark subclass without prepare method as prepared

    if (feature_class->prepare)
      prepared = feature_class->prepare (self);

    priv->prepared = prepared;
  }
}

void
nyx_feature_call_unprepare (NyxFeature *self)
{
  NyxFeaturePrivate *priv = nyx_feature_get_instance_private (self);

  if (priv->prepared) {
    NyxFeatureClass *feature_class = NYX_FEATURE_GET_CLASS (self);
    gboolean unprepared = TRUE; // mark subclass without unprepare method as unprepared

    if (feature_class->unprepare)
      unprepared = feature_class->unprepare (self);

    priv->prepared = !unprepared;
  }
}

void
nyx_feature_call_property_changed (NyxFeature *self, GParamSpec *pspec)
{
  CALL_WITH_ARGS (self, property_changed, pspec);
}

void
nyx_feature_call_state_changed (NyxFeature *self, NyxPlayerState state)
{
  CALL_WITH_ARGS (self, state_changed, state);
}

void
nyx_feature_call_position_changed (NyxFeature *self, gdouble position)
{
  CALL_WITH_ARGS (self, position_changed, position);
}

void
nyx_feature_call_speed_changed (NyxFeature *self, gdouble speed)
{
  CALL_WITH_ARGS (self, speed_changed, speed);
}

void
nyx_feature_call_volume_changed (NyxFeature *self, gdouble volume)
{
  CALL_WITH_ARGS (self, volume_changed, volume);
}

void
nyx_feature_call_mute_changed (NyxFeature *self, gboolean mute)
{
  CALL_WITH_ARGS (self, mute_changed, mute);
}

void
nyx_feature_call_played_item_changed (NyxFeature *self, NyxMediaItem *item)
{
  CALL_WITH_ARGS (self, played_item_changed, item);
}

void
nyx_feature_call_item_updated (NyxFeature *self, NyxMediaItem *item)
{
  CALL_WITH_ARGS (self, item_updated, item);
}

void
nyx_feature_call_queue_item_added (NyxFeature *self, NyxMediaItem *item, guint index)
{
  CALL_WITH_ARGS (self, queue_item_added, item, index);
}

void
nyx_feature_call_queue_item_removed (NyxFeature *self, NyxMediaItem *item, guint index)
{
  CALL_WITH_ARGS (self, queue_item_removed, item, index);
}

void
nyx_feature_call_queue_item_repositioned (NyxFeature *self, guint before, guint after)
{
  CALL_WITH_ARGS (self, queue_item_repositioned, before, after);
}

void
nyx_feature_call_queue_cleared (NyxFeature *self)
{
  CALL_WITHOUT_ARGS (self, queue_cleared);
}

void
nyx_feature_call_queue_progression_changed (NyxFeature *self, NyxQueueProgressionMode mode)
{
  CALL_WITH_ARGS (self, queue_progression_changed, mode);
}

static void
nyx_feature_init (NyxFeature *self)
{
}

static void
nyx_feature_dispatch_properties_changed (GObject *object,
    guint n_pspecs, GParamSpec **pspecs)
{
  NyxPlayer *player;

  if ((player = NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (object))))) {
    NyxFeaturesManager *features_manager;

    if ((features_manager = nyx_player_get_features_manager (player))) {
      guint i;

      for (i = 0; i < n_pspecs; ++i) {
        nyx_features_manager_trigger_property_changed (features_manager,
            NYX_FEATURE_CAST (object), pspecs[i]);
      }
    }

    gst_object_unref (player);
  }

  G_OBJECT_CLASS (parent_class)->dispatch_properties_changed (object, n_pspecs, pspecs);
}

static void
nyx_feature_finalize (GObject *object)
{
  NyxFeature *self = NYX_FEATURE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_feature_class_init (NyxFeatureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxfeature", 0,
      "Nyx Feature");

  gobject_class->dispatch_properties_changed = nyx_feature_dispatch_properties_changed;
  gobject_class->finalize = nyx_feature_finalize;
}
