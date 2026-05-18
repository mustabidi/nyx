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
 * NyxExtractable:
 *
 * An interface for creating enhancers that resolve given URI into something playable.
 *
 * Since: 0.8
 */

#include <gst/gst.h>

#include "nyx-extractable-private.h"
#include "nyx-harvest-private.h"

G_DEFINE_INTERFACE (NyxExtractable, nyx_extractable, G_TYPE_OBJECT);

static gboolean
nyx_extractable_default_extract (NyxExtractable *self, GUri *uri,
    NyxHarvest *harvest, GCancellable *cancellable, GError **error)
{
  if (*error == NULL) {
    g_set_error (error, GST_CORE_ERROR,
        GST_CORE_ERROR_NOT_IMPLEMENTED,
        "Extractable object did not implement extract function");
  }

  return FALSE;
}

static void
nyx_extractable_default_init (NyxExtractableInterface *iface)
{
  iface->extract = nyx_extractable_default_extract;
}

gboolean
nyx_extractable_extract (NyxExtractable *self, GUri *uri,
    NyxHarvest *harvest, GCancellable *cancellable, GError **error)
{
  NyxExtractableInterface *iface = NYX_EXTRACTABLE_GET_IFACE (self);

  return iface->extract (self, uri, harvest, cancellable, error);
}
