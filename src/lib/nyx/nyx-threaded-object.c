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
 * NyxThreadedObject:
 *
 * A base class for creating objects that work within a separate thread.
 */

#include "nyx-threaded-object.h"

#define GST_CAT_DEFAULT nyx_threaded_object_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _NyxThreadedObjectPrivate NyxThreadedObjectPrivate;

struct _NyxThreadedObjectPrivate
{
  GMutex lock; // Separate mutex to not deadlock with subclass on wakeups
  GCond cond;
  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;
  gboolean started;
};

#define parent_class nyx_threaded_object_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (NyxThreadedObject, nyx_threaded_object, GST_TYPE_OBJECT)

/**
 * nyx_threaded_object_get_context:
 * @threaded_object: a #NyxThreadedObject
 *
 * Get the #GMainContext of the thread used by this object.
 *
 * Useful when you want to invoke object thread to do some
 * action in it from a different thread.
 *
 * Returns: (transfer none): a #GMainContext of the object used thread.
 */
GMainContext *
nyx_threaded_object_get_context (NyxThreadedObject *self)
{
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  return priv->context;
}

static gboolean
main_loop_running_cb (NyxThreadedObject *self)
{
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&priv->lock);
  priv->started = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
nyx_threaded_object_main (NyxThreadedObject *self)
{
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);
  NyxThreadedObjectClass *threaded_object_class = NYX_THREADED_OBJECT_GET_CLASS (self);
  const gchar *obj_cls_name = G_OBJECT_CLASS_NAME (threaded_object_class);
  GSource *idle_source;

  GST_TRACE_OBJECT (self, "%s thread: %p", obj_cls_name, g_thread_self ());

  priv->context = g_main_context_new ();
  priv->loop = g_main_loop_new (priv->context, FALSE);

  g_main_context_push_thread_default (priv->context);

  if (threaded_object_class->thread_start)
    threaded_object_class->thread_start (self);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) main_loop_running_cb, self, NULL);
  g_source_attach (idle_source, priv->context);
  g_source_unref (idle_source);

  GST_DEBUG_OBJECT (self, "%s main loop running", obj_cls_name);
  g_main_loop_run (priv->loop);
  GST_DEBUG_OBJECT (self, "%s main loop stopped", obj_cls_name);

  if (threaded_object_class->thread_stop)
    threaded_object_class->thread_stop (self);

  g_main_context_pop_thread_default (priv->context);
  g_main_context_unref (priv->context);

  return NULL;
}

static void
nyx_threaded_object_init (NyxThreadedObject *self)
{
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  g_mutex_init (&priv->lock);
  g_cond_init (&priv->cond);
}

static void
nyx_threaded_object_constructed (GObject *object)
{
  NyxThreadedObject *self = NYX_THREADED_OBJECT_CAST (object);
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  GST_TRACE_OBJECT (self, "Constructed from thread: %p", g_thread_self ());

  g_mutex_lock (&priv->lock);

  priv->thread = g_thread_new (GST_OBJECT_NAME (object),
      (GThreadFunc) nyx_threaded_object_main, self);
  while (!priv->started)
    g_cond_wait (&priv->cond, &priv->lock);

  g_mutex_unlock (&priv->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
nyx_threaded_object_dispose (GObject *object)
{
  NyxThreadedObject *self = NYX_THREADED_OBJECT_CAST (object);
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  g_mutex_lock (&priv->lock);

  if (priv->loop) {
    g_main_loop_quit (priv->loop);

    if (G_LIKELY (priv->thread != g_thread_self ()))
      g_thread_join (priv->thread);
    else
      g_thread_unref (priv->thread);

    g_clear_pointer (&priv->loop, g_main_loop_unref);
  }

  g_mutex_unlock (&priv->lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_threaded_object_finalize (GObject *object)
{
  NyxThreadedObject *self = NYX_THREADED_OBJECT_CAST (object);
  NyxThreadedObjectPrivate *priv = nyx_threaded_object_get_instance_private (self);

  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_threaded_object_class_init (NyxThreadedObjectClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxthreadedobject", 0,
      "Nyx Threaded Object");

  gobject_class->constructed = nyx_threaded_object_constructed;
  gobject_class->dispose = nyx_threaded_object_dispose;
  gobject_class->finalize = nyx_threaded_object_finalize;
}
