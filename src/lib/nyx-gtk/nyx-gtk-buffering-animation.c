/* Nyx GTK Integration Library
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

#include <nyx/nyx.h>

#include "nyx-gtk-buffering-animation-private.h"
#include "nyx-gtk-buffering-paintable-private.h"

#define MIN_STEP_DELAY 30000

#define GST_CAT_DEFAULT nyx_gtk_buffering_animation_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkBufferingAnimation
{
  NyxGtkContainer parent;

  NyxGtkBufferingPaintable *buffering_paintable;

  guint tick_id;
  gint64 last_tick;
};

#define parent_class nyx_gtk_buffering_animation_parent_class
G_DEFINE_TYPE (NyxGtkBufferingAnimation, nyx_gtk_buffering_animation, NYX_GTK_TYPE_CONTAINER)

static gboolean
_animation_tick (GtkWidget *picture, GdkFrameClock *frame_clock, NyxGtkBufferingAnimation *self)
{
  gint64 now = gdk_frame_clock_get_frame_time (frame_clock);

  /* We do not want for this animation to move too fast */
  if (now - self->last_tick >= MIN_STEP_DELAY) {
    GST_LOG_OBJECT (self, "Animation step, last: %" G_GINT64_FORMAT
        ", now: %" G_GINT64_FORMAT, self->last_tick, now);
    nyx_gtk_buffering_paintable_step (self->buffering_paintable);
    self->last_tick = now;
  }

  return G_SOURCE_CONTINUE;
}

void
nyx_gtk_buffering_animation_start (NyxGtkBufferingAnimation *self)
{
  GtkWidget *picture;

  if (self->tick_id != 0)
    return;

  GST_DEBUG_OBJECT (self, "Animation start");

  picture = nyx_gtk_container_get_child (NYX_GTK_CONTAINER (self));
  self->tick_id = gtk_widget_add_tick_callback (picture,
      (GtkTickCallback) _animation_tick, self, NULL);
}

void
nyx_gtk_buffering_animation_stop (NyxGtkBufferingAnimation *self)
{
  GtkWidget *picture;

  if (self->tick_id == 0)
    return;

  GST_DEBUG_OBJECT (self, "Animation stop");

  picture = nyx_gtk_container_get_child (NYX_GTK_CONTAINER (self));
  gtk_widget_remove_tick_callback (picture, self->tick_id);

  self->tick_id = 0;
  self->last_tick = 0;
  nyx_gtk_buffering_paintable_reset (self->buffering_paintable);
}

static void
nyx_gtk_buffering_animation_init (NyxGtkBufferingAnimation *self)
{
  GtkWidget *picture = gtk_picture_new ();
  self->buffering_paintable = nyx_gtk_buffering_paintable_new ();

  gtk_picture_set_paintable (GTK_PICTURE (picture),
      GDK_PAINTABLE (self->buffering_paintable));

  nyx_gtk_container_set_child (NYX_GTK_CONTAINER (self), picture);
}

static void
nyx_gtk_buffering_animation_unmap (GtkWidget *widget)
{
  NyxGtkBufferingAnimation *self = NYX_GTK_BUFFERING_ANIMATION_CAST (widget);

  nyx_gtk_buffering_animation_stop (self);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
nyx_gtk_buffering_animation_finalize (GObject *object)
{
  NyxGtkBufferingAnimation *self = NYX_GTK_BUFFERING_ANIMATION_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->buffering_paintable);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_gtk_buffering_animation_class_init (NyxGtkBufferingAnimationClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtkbufferinganimation", 0,
      "Nyx GTK Buffering Animation");

  gobject_class->finalize = nyx_gtk_buffering_animation_finalize;

  widget_class->unmap = nyx_gtk_buffering_animation_unmap;

  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-buffering-animation");
}
