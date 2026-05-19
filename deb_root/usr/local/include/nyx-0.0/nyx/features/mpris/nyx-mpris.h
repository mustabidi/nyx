/*
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
#include <nyx/nyx-feature.h>

G_BEGIN_DECLS

#define NYX_TYPE_MPRIS (nyx_mpris_get_type())
#define NYX_MPRIS_CAST(obj) ((NyxMpris *)(obj))

NYX_DEPRECATED
G_DECLARE_FINAL_TYPE (NyxMpris, nyx_mpris, NYX, MPRIS, NyxFeature)

NYX_DEPRECATED
NyxMpris * nyx_mpris_new (const gchar *own_name, const gchar *identity, const gchar *desktop_entry);

NYX_DEPRECATED
void nyx_mpris_set_queue_controllable (NyxMpris *mpris, gboolean controllable);

NYX_DEPRECATED
gboolean nyx_mpris_get_queue_controllable (NyxMpris *mpris);

NYX_DEPRECATED
void nyx_mpris_set_fallback_art_url (NyxMpris *mpris, const gchar *art_url);

NYX_DEPRECATED
gchar * nyx_mpris_get_fallback_art_url (NyxMpris *mpris);

G_END_DECLS
