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
 * NyxStream:
 *
 * Represents a stream within media.
 */

#include <gst/tag/tag.h>

#include "nyx-stream-private.h"
#include "nyx-player-private.h"

#define GST_CAT_DEFAULT nyx_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _NyxStreamPrivate NyxStreamPrivate;

struct _NyxStreamPrivate
{
  GstStream *gst_stream;

  NyxStreamType stream_type;
  gchar *title;
};

enum
{
  PROP_0,
  PROP_STREAM_TYPE,
  PROP_TITLE,
  PROP_LAST
};

#define parent_class nyx_stream_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (NyxStream, nyx_stream, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_update_using_tags (NyxStream *self, GstTagList *tags)
{
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (self);
  gchar *title = NULL;

  gst_tag_list_get_string_index (tags, GST_TAG_TITLE, 0, &title);
  nyx_stream_take_string_prop (self, param_specs[PROP_TITLE], &priv->title, title);
}

NyxStream *
nyx_stream_new (GstStream *gst_stream)
{
  NyxStream *stream;
  NyxStreamPrivate *priv;

  stream = g_object_new (NYX_TYPE_STREAM, NULL);
  gst_object_ref_sink (stream);

  priv = nyx_stream_get_instance_private (stream);
  priv->gst_stream = gst_object_ref (gst_stream);

  return stream;
}

/*
 * This should be called only during stream construction
 */
void
nyx_stream_set_gst_stream (NyxStream *stream, GstStream *gst_stream)
{
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (stream);

  if (G_LIKELY (gst_object_replace ((GstObject **) &priv->gst_stream,
      GST_OBJECT_CAST (gst_stream)))) {
    GstCaps *caps = gst_stream_get_caps (gst_stream);
    GstTagList *tags = gst_stream_get_tags (gst_stream);

    if (caps || tags) {
      NyxStreamClass *stream_class = NYX_STREAM_GET_CLASS (stream);

      stream_class->internal_stream_updated (stream, caps, tags);

      gst_clear_caps (&caps);
      gst_clear_tag_list (&tags);
    }
  }
}

/**
 * nyx_stream_get_stream_type:
 * @stream: a #NyxStream
 *
 * Get the #NyxStreamType of @stream.
 *
 * Returns: type of stream.
 */
NyxStreamType
nyx_stream_get_stream_type (NyxStream *self)
{
  NyxStreamPrivate *priv;

  g_return_val_if_fail (NYX_IS_STREAM (self), NYX_STREAM_TYPE_UNKNOWN);

  priv = nyx_stream_get_instance_private (self);

  return priv->stream_type;
}

/**
 * nyx_stream_get_title:
 * @stream: a #NyxStream
 *
 * Get the title of @stream, if any.
 *
 * Returns: (transfer full) (nullable): title of stream.
 */
gchar *
nyx_stream_get_title (NyxStream *self)
{
  NyxStreamPrivate *priv;
  gchar *title;

  g_return_val_if_fail (NYX_IS_STREAM (self), NULL);

  priv = nyx_stream_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  title = g_strdup (priv->title);
  GST_OBJECT_UNLOCK (self);

  return title;
}

GstStream *
nyx_stream_get_gst_stream (NyxStream *self)
{
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (self);

  return priv->gst_stream;
}

static void
nyx_stream_prop_notify (NyxStream *self, GParamSpec *pspec)
{
  NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

  /* NOTE: This happens when props are initially set during construction
   * (before parented) which is fine, since we do not have to notify
   * when user does not have access to object yet. */
  if (!player)
    return;

  nyx_app_bus_post_prop_notify (player->app_bus,
      GST_OBJECT_CAST (self), pspec);

  gst_object_unref (player);
}

void
nyx_stream_take_string_prop (NyxStream *self,
    GParamSpec *pspec, gchar **ptr, gchar *value)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = g_strcmp0 (*ptr, value) != 0)) {
    g_free (*ptr);
    *ptr = value;

    GST_DEBUG_OBJECT (self, "Set %s: %s",
        g_param_spec_get_name (pspec), value);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    nyx_stream_prop_notify (self, pspec);
  else
    g_free (value);
}

void
nyx_stream_set_string_prop (NyxStream *self,
    GParamSpec *pspec, gchar **ptr, const gchar *value)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = g_set_str (ptr, value))) {
    GST_DEBUG_OBJECT (self, "Set %s: %s",
        g_param_spec_get_name (pspec), value);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    nyx_stream_prop_notify (self, pspec);
}

void
nyx_stream_set_int_prop (NyxStream *self,
    GParamSpec *pspec, gint *ptr, gint value)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = *ptr != value)) {
    *ptr = value;

    GST_DEBUG_OBJECT (self, "Set %s: %i",
        g_param_spec_get_name (pspec), value);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    nyx_stream_prop_notify (self, pspec);
}

void
nyx_stream_set_uint_prop (NyxStream *self,
    GParamSpec *pspec, guint *ptr, guint value)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = *ptr != value)) {
    *ptr = value;

    GST_DEBUG_OBJECT (self, "Set %s: %u",
        g_param_spec_get_name (pspec), value);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    nyx_stream_prop_notify (self, pspec);
}

void
nyx_stream_set_double_prop (NyxStream *self,
    GParamSpec *pspec, gdouble *ptr, gdouble value)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (*ptr, value, FLT_EPSILON))) {
    *ptr = value;

    GST_DEBUG_OBJECT (self, "Set %s: %lf",
        g_param_spec_get_name (pspec), value);
  }
  GST_OBJECT_UNLOCK (self);

  if (changed)
    nyx_stream_prop_notify (self, pspec);
}

static void
nyx_stream_init (NyxStream *self)
{
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (self);

  priv->stream_type = NYX_STREAM_TYPE_UNKNOWN;
}

static void
nyx_stream_finalize (GObject *object)
{
  NyxStream *self = NYX_STREAM_CAST (object);
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&priv->gst_stream);

  g_free (priv->title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxStream *self = NYX_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_STREAM_TYPE:
      g_value_set_enum (value, nyx_stream_get_stream_type (self));
      break;
    case PROP_TITLE:
      g_value_take_string (value, nyx_stream_get_title (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_stream_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxStream *self = NYX_STREAM_CAST (object);
  NyxStreamPrivate *priv = nyx_stream_get_instance_private (self);

  switch (prop_id) {
    case PROP_STREAM_TYPE:
      priv->stream_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_stream_internal_stream_updated (NyxStream *self,
    GstCaps *caps, GstTagList *tags)
{
  if (caps)
    GST_LOG_OBJECT (self, "Caps: %" GST_PTR_FORMAT, caps);
  if (tags) {
    GST_LOG_OBJECT (self, "Tags: %" GST_PTR_FORMAT, tags);
    _update_using_tags (self, tags);
  }
}

static void
nyx_stream_class_init (NyxStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxStreamClass *stream_class = (NyxStreamClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxstream", 0,
      "Nyx Stream");

  gobject_class->get_property = nyx_stream_get_property;
  gobject_class->set_property = nyx_stream_set_property;
  gobject_class->finalize = nyx_stream_finalize;

  stream_class->internal_stream_updated = nyx_stream_internal_stream_updated;

  /**
   * NyxStream:stream-type:
   *
   * Type of stream.
   */
  param_specs[PROP_STREAM_TYPE] = g_param_spec_enum ("stream-type",
      NULL, NULL, NYX_TYPE_STREAM_TYPE, NYX_STREAM_TYPE_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxStream:title:
   *
   * Title of stream.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
