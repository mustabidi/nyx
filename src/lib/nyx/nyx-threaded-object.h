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

#pragma once

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>

G_BEGIN_DECLS

#define NYX_TYPE_THREADED_OBJECT (nyx_threaded_object_get_type())
#define NYX_THREADED_OBJECT_CAST(obj) ((NyxThreadedObject *)(obj))

NYX_API
G_DECLARE_DERIVABLE_TYPE (NyxThreadedObject, nyx_threaded_object, NYX, THREADED_OBJECT, GstObject)

/**
 * NyxThreadedObjectClass:
 * @parent_class: The object class structure.
 * @thread_start: Called right after thread started.
 * @thread_stop: Called when thread is going to stop.
 */
struct _NyxThreadedObjectClass
{
  GstObjectClass parent_class;

  /**
   * NyxThreadedObjectClass::thread_start:
   * @threaded_object: a #NyxThreadedObject
   *
   * Called right after thread started.
   *
   * Useful for initializing objects that work within this new thread.
   */
  void (* thread_start) (NyxThreadedObject *threaded_object);

  /**
   * NyxThreadedObjectClass::thread_stop:
   * @threaded_object: a #NyxThreadedObject
   *
   * Called when thread is going to stop.
   *
   * Useful for cleanup of things created on thread start.
   */
  void (* thread_stop) (NyxThreadedObject *threaded_object);

  /*< private >*/
  gpointer padding[4];
};

NYX_API
GMainContext * nyx_threaded_object_get_context (NyxThreadedObject *threaded_object);

G_END_DECLS
