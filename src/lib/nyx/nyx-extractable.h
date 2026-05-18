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
#include <gio/gio.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-harvest.h>

G_BEGIN_DECLS

#define NYX_TYPE_EXTRACTABLE (nyx_extractable_get_type())
#define NYX_EXTRACTABLE_CAST(obj) ((NyxExtractable *)(obj))

NYX_API
G_DECLARE_INTERFACE (NyxExtractable, nyx_extractable, NYX, EXTRACTABLE, GObject)

/**
 * NyxExtractableInterface:
 * @parent_iface: The parent interface structure.
 * @extract: Extract data and fill harvest.
 */
struct _NyxExtractableInterface
{
  GTypeInterface parent_iface;

  /**
   * NyxExtractableInterface::extract:
   * @extractable: a #NyxExtractable
   * @uri: a #GUri
   * @harvest: a #NyxHarvest to be filled
   * @cancellable: (not nullable): a #GCancellable object
   * @error: (not nullable): a #GError
   *
   * Extract data and fill harvest.
   *
   * Returns: whether extraction was successful.
   *
   * Since: 0.8
   */
  gboolean (* extract) (NyxExtractable *extractable, GUri *uri, NyxHarvest *harvest, GCancellable *cancellable, GError **error);

  /*< private >*/
  gpointer padding[8];
};

G_END_DECLS
