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
 * NyxVideoStream:
 *
 * Represents a video stream within media.
 */

#include "nyx-video-stream-private.h"
#include "nyx-stream-private.h"

#define GST_CAT_DEFAULT nyx_video_stream_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxVideoStream
{
  NyxStream parent;

  gchar *codec;
  gint width;
  gint height;
  gdouble fps;
  guint bitrate;
  gchar *pixel_format;
};

#define parent_class nyx_video_stream_parent_class
G_DEFINE_TYPE (NyxVideoStream, nyx_video_stream, NYX_TYPE_STREAM);

enum
{
  PROP_0,
  PROP_CODEC,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FPS,
  PROP_BITRATE,
  PROP_PIXEL_FORMAT,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_update_using_caps (NyxVideoStream *self, GstCaps *caps)
{
  NyxStream *stream = NYX_STREAM_CAST (self);
  GstStructure *structure;
  gint width = 0, height = 0, fps_n = 0, fps_d = 0;

  if (gst_caps_get_size (caps) == 0)
    return;

  structure = gst_caps_get_structure (caps, 0);

  /* NOTE: We cannot use gst_structure_get() here,
   * as it stops iterating on first not found key */

  gst_structure_get_int (structure, "width", &width);
  nyx_stream_set_int_prop (stream, param_specs[PROP_WIDTH], &self->width, width);

  gst_structure_get_int (structure, "height", &height);
  nyx_stream_set_int_prop (stream, param_specs[PROP_HEIGHT], &self->height, height);

  gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d);

  if (G_UNLIKELY (fps_d == 0))
    fps_d = 1;

  nyx_stream_set_double_prop (stream, param_specs[PROP_FPS], &self->fps, (gdouble) fps_n / fps_d);

  nyx_stream_set_string_prop (stream, param_specs[PROP_PIXEL_FORMAT], &self->pixel_format,
      gst_structure_get_string (structure, "format"));
}

static void
_update_using_tags (NyxVideoStream *self, GstTagList *tags)
{
  NyxStream *stream = NYX_STREAM_CAST (self);
  gchar *codec = NULL;
  guint bitrate = 0;

  gst_tag_list_get_string_index (tags, GST_TAG_VIDEO_CODEC, 0, &codec);
  nyx_stream_take_string_prop (stream, param_specs[PROP_CODEC], &self->codec, codec);

  gst_tag_list_get_uint_index (tags, GST_TAG_BITRATE, 0, &bitrate);
  nyx_stream_set_uint_prop (stream, param_specs[PROP_BITRATE], &self->bitrate, bitrate);
}

NyxStream *
nyx_video_stream_new (GstStream *gst_stream)
{
  NyxVideoStream *video_stream;

  video_stream = g_object_new (NYX_TYPE_VIDEO_STREAM,
      "stream-type", NYX_STREAM_TYPE_VIDEO, NULL);
  gst_object_ref_sink (video_stream);

  nyx_stream_set_gst_stream (NYX_STREAM_CAST (video_stream), gst_stream);

  return NYX_STREAM_CAST (video_stream);
}

/**
 * nyx_video_stream_get_codec:
 * @stream: a #NyxVideoStream
 *
 * Get codec used to encode @stream.
 *
 * Returns: (transfer full) (nullable): the video codec of stream
 *   or %NULL if undetermined.
 */
gchar *
nyx_video_stream_get_codec (NyxVideoStream *self)
{
  gchar *codec;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  codec = g_strdup (self->codec);
  GST_OBJECT_UNLOCK (self);

  return codec;
}

/**
 * nyx_video_stream_get_width:
 * @stream: a #NyxVideoStream
 *
 * Get width of video @stream.
 *
 * Returns: the width of video stream.
 */
gint
nyx_video_stream_get_width (NyxVideoStream *self)
{
  gint width;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  width = self->width;
  GST_OBJECT_UNLOCK (self);

  return width;
}

/**
 * nyx_video_stream_get_height:
 * @stream: a #NyxVideoStream
 *
 * Get height of video @stream.
 *
 * Returns: the height of video stream.
 */
gint
nyx_video_stream_get_height (NyxVideoStream *self)
{
  gint height;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  height = self->height;
  GST_OBJECT_UNLOCK (self);

  return height;
}

/**
 * nyx_video_stream_get_fps:
 * @stream: a #NyxVideoStream
 *
 * Get number of frames per second in video @stream.
 *
 * Returns: the FPS of video stream.
 */
gdouble
nyx_video_stream_get_fps (NyxVideoStream *self)
{
  gdouble fps;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  fps = self->fps;
  GST_OBJECT_UNLOCK (self);

  return fps;
}

/**
 * nyx_video_stream_get_bitrate:
 * @stream: a #NyxVideoStream
 *
 * Get bitrate of video @stream.
 *
 * Returns: the bitrate of video stream.
 */
guint
nyx_video_stream_get_bitrate (NyxVideoStream *self)
{
  guint bitrate;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), 0);

  GST_OBJECT_LOCK (self);
  bitrate = self->bitrate;
  GST_OBJECT_UNLOCK (self);

  return bitrate;
}

/**
 * nyx_video_stream_get_pixel_format:
 * @stream: a #NyxVideoStream
 *
 * Get pixel format of video @stream.
 *
 * Returns: (transfer full) (nullable): the pixel format of stream
 *   or %NULL if undetermined.
 */
gchar *
nyx_video_stream_get_pixel_format (NyxVideoStream *self)
{
  gchar *pixel_format;

  g_return_val_if_fail (NYX_IS_VIDEO_STREAM (self), NULL);

  GST_OBJECT_LOCK (self);
  pixel_format = g_strdup (self->pixel_format);
  GST_OBJECT_UNLOCK (self);

  return pixel_format;
}

static void
nyx_video_stream_init (NyxVideoStream *self)
{
}

static void
nyx_video_stream_finalize (GObject *object)
{
  NyxVideoStream *self = NYX_VIDEO_STREAM_CAST (object);

  g_free (self->codec);
  g_free (self->pixel_format);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_video_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxVideoStream *self = NYX_VIDEO_STREAM_CAST (object);

  switch (prop_id) {
    case PROP_CODEC:
      g_value_take_string (value, nyx_video_stream_get_codec (self));
      break;
    case PROP_WIDTH:
      g_value_set_int (value, nyx_video_stream_get_width (self));
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, nyx_video_stream_get_height (self));
      break;
    case PROP_FPS:
      g_value_set_double (value, nyx_video_stream_get_fps (self));
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, nyx_video_stream_get_bitrate (self));
      break;
    case PROP_PIXEL_FORMAT:
      g_value_take_string (value, nyx_video_stream_get_pixel_format (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_video_stream_internal_stream_updated (NyxStream *stream,
    GstCaps *caps, GstTagList *tags)
{
  NyxVideoStream *self = NYX_VIDEO_STREAM_CAST (stream);

  NYX_STREAM_CLASS (parent_class)->internal_stream_updated (stream, caps, tags);

  if (caps)
    _update_using_caps (self, caps);
  if (tags)
    _update_using_tags (self, tags);
}

static void
nyx_video_stream_class_init (NyxVideoStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  NyxStreamClass *stream_class = (NyxStreamClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxvideostream", 0,
      "Nyx Video Stream");

  gobject_class->get_property = nyx_video_stream_get_property;
  gobject_class->finalize = nyx_video_stream_finalize;

  stream_class->internal_stream_updated = nyx_video_stream_internal_stream_updated;

  /**
   * NyxVideoStream:codec:
   *
   * Stream codec.
   */
  param_specs[PROP_CODEC] = g_param_spec_string ("codec",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxVideoStream:width:
   *
   * Stream width.
   */
  param_specs[PROP_WIDTH] = g_param_spec_int ("width",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxVideoStream:height:
   *
   * Stream height.
   */
  param_specs[PROP_HEIGHT] = g_param_spec_int ("height",
      NULL, NULL, 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxVideoStream:fps:
   *
   * Stream FPS.
   */
  param_specs[PROP_FPS] = g_param_spec_double ("fps",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxVideoStream:bitrate:
   *
   * Stream bitrate.
   */
  param_specs[PROP_BITRATE] = g_param_spec_uint ("bitrate",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxVideoStream:pixel-format:
   *
   * Stream pixel format.
   */
  param_specs[PROP_PIXEL_FORMAT] = g_param_spec_string ("pixel-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
