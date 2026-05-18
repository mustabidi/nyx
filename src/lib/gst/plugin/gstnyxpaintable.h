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
#include <gst/video/video.h>

#include "gstnyximporter.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_PAINTABLE (gst_nyx_paintable_get_type())
G_DECLARE_FINAL_TYPE (GstNyxPaintable, gst_nyx_paintable, GST, NYX_PAINTABLE, GObject)

#define GST_NYX_PAINTABLE_CAST(obj)          ((GstNyxPaintable *)(obj))

#define GST_NYX_PAINTABLE_GET_LOCK(obj)      (&GST_NYX_PAINTABLE_CAST(obj)->lock)
#define GST_NYX_PAINTABLE_LOCK(obj)          g_mutex_lock (GST_NYX_PAINTABLE_GET_LOCK(obj))
#define GST_NYX_PAINTABLE_UNLOCK(obj)        g_mutex_unlock (GST_NYX_PAINTABLE_GET_LOCK(obj))

#define GST_NYX_PAINTABLE_IMPORTER_GET_LOCK(obj)      (&GST_NYX_PAINTABLE_CAST(obj)->importer_lock)
#define GST_NYX_PAINTABLE_IMPORTER_LOCK(obj)          g_mutex_lock (GST_NYX_PAINTABLE_IMPORTER_GET_LOCK(obj))
#define GST_NYX_PAINTABLE_IMPORTER_UNLOCK(obj)        g_mutex_unlock (GST_NYX_PAINTABLE_IMPORTER_GET_LOCK(obj))

struct _GstNyxPaintable
{
  GObject parent;

  GMutex lock;
  GMutex importer_lock;

  GstVideoInfo v_info;

  GdkRGBA bg;

  GWeakRef widget;
  GstNyxImporter *importer;

  /* Sink properties */
  gint par_n, par_d;
  GstVideoOrientationMethod rotation;

  /* Resize */
  gboolean pending_resize;
  guint display_ratio_num;
  guint display_ratio_den;

  /* GdkPaintableInterface */
  gint display_width;
  gint display_height;
  gdouble display_aspect_ratio;

  /* Pending draw signal id */
  guint draw_id;
};

GstNyxPaintable *      gst_nyx_paintable_new                    (void);
void                       gst_nyx_paintable_queue_draw             (GstNyxPaintable *paintable);
void                       gst_nyx_paintable_set_widget             (GstNyxPaintable *paintable, GtkWidget *widget);
void                       gst_nyx_paintable_set_importer           (GstNyxPaintable *paintable, GstNyxImporter *importer);
gboolean                   gst_nyx_paintable_set_video_info         (GstNyxPaintable *paintable, const GstVideoInfo *v_info);
void                       gst_nyx_paintable_set_pixel_aspect_ratio (GstNyxPaintable *paintable, gint par_n, gint par_d);
void                       gst_nyx_paintable_set_rotation           (GstNyxPaintable *paintable, GstVideoOrientationMethod rotation);
GstVideoOrientationMethod  gst_nyx_paintable_get_rotation           (GstNyxPaintable *paintable);

G_END_DECLS
