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
 * NyxMarker:
 *
 * Represents a point in timeline.
 *
 * Markers are a convienient way of marking points of interest within a
 * [class@Nyx.Timeline] of [class@Nyx.MediaItem]. Use them
 * to indicate certain areas on the timeline.
 *
 * Markers are reference counted immutable objects. Once a marker is created
 * it can only be inserted into a single [class@Nyx.Timeline] at a time.
 *
 * Please note that markers are independent of [property@Nyx.MediaItem:duration]
 * and applications should not assume that all markers must have start/end times
 * lower or equal the item duration. This is not the case in e.g. live streams
 * where duration is unknown, but markers are still allowed to mark entries
 * (like EPG titles for example).
 *
 * Remember that [class@Nyx.Player] will also automatically insert certain
 * markers extracted from media such as video chapters. Nyx will never
 * "touch" the ones created by the application. If you want to differentiate
 * your own markers, applications can define and create markers with one of
 * the custom types from [enum@Nyx.MarkerType] enum.
 *
 * Example:
 *
 * ```c
 * #define MY_APP_MARKER (NYX_MARKER_TYPE_CUSTOM_1)
 *
 * NyxMarker *marker = nyx_marker_new (MY_APP_MARKER, title, start, end);
 * ```
 *
 * ```c
 * NyxMarkerType marker_type = nyx_marker_get_marker_type (marker);
 *
 * if (marker_type == MY_APP_MARKER) {
 *   // Do something with your custom marker
 * }
 * ```
 */

#include "nyx-marker-private.h"

#define GST_CAT_DEFAULT nyx_marker_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxMarker
{
  GstObject parent;

  NyxMarkerType marker_type;
  gchar *title;
  gdouble start;
  gdouble end;

  gboolean is_internal;
};

enum
{
  PROP_0,
  PROP_MARKER_TYPE,
  PROP_TITLE,
  PROP_START,
  PROP_END,
  PROP_LAST
};

#define parent_class nyx_marker_parent_class
G_DEFINE_TYPE (NyxMarker, nyx_marker, GST_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * nyx_marker_new:
 * @marker_type: a #NyxMarkerType
 * @title: (nullable): title of the marker
 * @start: a start position of the marker
 * @end: an end position of the marker or [const@Nyx.MARKER_NO_END] if none
 *
 * Creates a new #NyxMarker with given params.
 *
 * It is considered a programmer error trying to set an ending
 * point that is before the starting one. If end is unknown or
 * not defined a special [const@Nyx.MARKER_NO_END] value
 * should be used.
 *
 * Returns: (transfer full): a new #NyxMarker.
 */
NyxMarker *
nyx_marker_new (NyxMarkerType marker_type, const gchar *title,
    gdouble start, gdouble end)
{
  NyxMarker *marker;

  marker = g_object_new (NYX_TYPE_MARKER,
      "marker-type", marker_type,
      "title", title,
      "start", start,
      "end", end, NULL);
  gst_object_ref_sink (marker);

  return marker;
}

NyxMarker *
nyx_marker_new_internal (NyxMarkerType marker_type, const gchar *title,
    gdouble start, gdouble end)
{
  NyxMarker *marker;

  marker = nyx_marker_new (marker_type, title, start, end);
  marker->is_internal = TRUE;

  return marker;
}

/**
 * nyx_marker_get_marker_type:
 * @marker: a #NyxMarker
 *
 * Get the #NyxMarkerType of @marker.
 *
 * Returns: type of marker.
 */
NyxMarkerType
nyx_marker_get_marker_type (NyxMarker *self)
{
  g_return_val_if_fail (NYX_IS_MARKER (self), NYX_MARKER_TYPE_UNKNOWN);

  return self->marker_type;
}

/**
 * nyx_marker_get_title:
 * @marker: a #NyxMarker
 *
 * Get the title of @marker.
 *
 * Returns: (nullable): the marker title.
 */
const gchar *
nyx_marker_get_title (NyxMarker *self)
{
  g_return_val_if_fail (NYX_IS_MARKER (self), NULL);

  return self->title;
}

/**
 * nyx_marker_get_start:
 * @marker: a #NyxMarker
 *
 * Get the start position (in seconds) of @marker.
 *
 * Returns: marker start.
 */
gdouble
nyx_marker_get_start (NyxMarker *self)
{
  g_return_val_if_fail (NYX_IS_MARKER (self), 0);

  return self->start;
}

/**
 * nyx_marker_get_end:
 * @marker: a #NyxMarker
 *
 * Get the end position (in seconds) of @marker.
 *
 * Returns: marker end.
 */
gdouble
nyx_marker_get_end (NyxMarker *self)
{
  g_return_val_if_fail (NYX_IS_MARKER (self), NYX_MARKER_NO_END);

  return self->end;
}

gboolean
nyx_marker_is_internal (NyxMarker *self)
{
  return self->is_internal;
}

static void
nyx_marker_init (NyxMarker *self)
{
  self->marker_type = NYX_MARKER_TYPE_UNKNOWN;
  self->end = NYX_MARKER_NO_END;
}

static void
nyx_marker_constructed (GObject *object)
{
  NyxMarker *self = NYX_MARKER_CAST (object);

  G_OBJECT_CLASS (parent_class)->constructed (object);

  GST_TRACE_OBJECT (self, "Created new marker"
      ", type: %i, title: \"%s\", start: %lf, end: %lf",
      self->marker_type, GST_STR_NULL (self->title), self->start, self->end);
}

static void
nyx_marker_finalize (GObject *object)
{
  NyxMarker *self = NYX_MARKER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_marker_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxMarker *self = NYX_MARKER_CAST (object);

  switch (prop_id) {
    case PROP_MARKER_TYPE:
      g_value_set_enum (value, nyx_marker_get_marker_type (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, nyx_marker_get_title (self));
      break;
    case PROP_START:
      g_value_set_double (value, nyx_marker_get_start (self));
      break;
    case PROP_END:
      g_value_set_double (value, nyx_marker_get_end (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_marker_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxMarker *self = NYX_MARKER_CAST (object);

  switch (prop_id) {
    case PROP_MARKER_TYPE:
      self->marker_type = g_value_get_enum (value);
      break;
    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;
    case PROP_START:
      self->start = g_value_get_double (value);
      break;
    case PROP_END:
      self->end = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_marker_class_init (NyxMarkerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxmarker", 0,
      "Nyx Marker");

  gobject_class->constructed = nyx_marker_constructed;
  gobject_class->get_property = nyx_marker_get_property;
  gobject_class->set_property = nyx_marker_set_property;
  gobject_class->finalize = nyx_marker_finalize;

  /**
   * NyxMarker:marker-type:
   *
   * Type of stream.
   */
  param_specs[PROP_MARKER_TYPE] = g_param_spec_enum ("marker-type",
      NULL, NULL, NYX_TYPE_MARKER_TYPE, NYX_MARKER_TYPE_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMarker:title:
   *
   * Title of marker.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMarker:start:
   *
   * Starting time of marker.
   */
  param_specs[PROP_START] = g_param_spec_double ("start",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMarker:end:
   *
   * Ending time of marker.
   */
  param_specs[PROP_END] = g_param_spec_double ("end",
      NULL, NULL, -1, G_MAXDOUBLE, NYX_MARKER_NO_END,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
