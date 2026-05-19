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
#include <nyx/nyx-enums.h>

G_BEGIN_DECLS

#define NYX_TYPE_STREAM (nyx_stream_get_type())
#define NYX_STREAM_CAST(obj) ((NyxStream *)(obj))

NYX_API
G_DECLARE_DERIVABLE_TYPE (NyxStream, nyx_stream, NYX, STREAM, GstObject)

struct _NyxStreamClass
{
  GstObjectClass parent_class;

  /**
   * NyxStreamClass::internal_stream_updated:
   * @stream: a #NyxStream
   * @caps: (nullable): an updated #GstCaps if changed
   * @tags: (nullable): an updated #GstTagList if changed
   *
   * This function is called when internal #GstStream gets updated.
   * Meant for internal usage only. Used for subclasses to update
   * their properties accordingly.
   *
   * Note that this vfunc is called from different threads.
   */
  void (* internal_stream_updated) (NyxStream *stream, GstCaps *caps, GstTagList *tags);

  /*< private >*/
  gpointer padding[4];
};

NYX_API
NyxStreamType nyx_stream_get_stream_type (NyxStream *stream);

NYX_API
gchar * nyx_stream_get_title (NyxStream *stream);

G_END_DECLS
