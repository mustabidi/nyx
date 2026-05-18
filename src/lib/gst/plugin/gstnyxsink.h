/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include "gstnyxpaintable.h"
#include "gstnyximporterloader.h"
#include "gstnyximporter.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_SINK (gst_nyx_sink_get_type())
G_DECLARE_FINAL_TYPE (GstNyxSink, gst_nyx_sink, GST, NYX_SINK, GstVideoSink)

#define GST_NYX_SINK_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_NYX_SINK, GstNyxSinkClass))
#define GST_NYX_SINK_CAST(obj)          ((GstNyxSink *)(obj))

#define GST_NYX_SINK_GET_LOCK(obj)      (&GST_NYX_SINK_CAST(obj)->lock)
#define GST_NYX_SINK_LOCK(obj)          g_mutex_lock (GST_NYX_SINK_GET_LOCK(obj))
#define GST_NYX_SINK_UNLOCK(obj)        g_mutex_unlock (GST_NYX_SINK_GET_LOCK(obj))

struct _GstNyxSink
{
  GstVideoSink parent;

  GMutex lock;

  GstNyxPaintable *paintable;
  GstNyxImporterLoader *loader;
  GstNyxImporter *importer;
  GstVideoInfo v_info;
  GstVideoOrientationMethod stream_orientation;

  GtkWidget *widget;
  GtkWindow *window;

  /* Properties */
  gboolean force_aspect_ratio;
  gint par_n, par_d;
  gboolean keep_last_frame;
  GstVideoOrientationMethod rotation_mode;

  /* Position coords */
  gdouble last_pos_x;
  gdouble last_pos_y;

  gulong widget_destroy_id;
  gulong window_destroy_id;
};

GST_ELEMENT_REGISTER_DECLARE (nyxsink);

G_END_DECLS
