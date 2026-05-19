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

#pragma once

#if !defined(__NYX_GTK_INSIDE__) && !defined(NYX_GTK_COMPILATION)
#error "Only <nyx-gtk/nyx-gtk.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <nyx-gtk/nyx-gtk-visibility.h>
#include <nyx-gtk/nyx-gtk-enums.h>
#include <nyx-gtk/nyx-gtk-container.h>

G_BEGIN_DECLS

#define NYX_GTK_TYPE_LEAD_CONTAINER (nyx_gtk_lead_container_get_type())
#define NYX_GTK_LEAD_CONTAINER_CAST(obj) ((NyxGtkLeadContainer *)(obj))

NYX_GTK_API
G_DECLARE_DERIVABLE_TYPE (NyxGtkLeadContainer, nyx_gtk_lead_container, NYX_GTK, LEAD_CONTAINER, NyxGtkContainer)

struct _NyxGtkLeadContainerClass
{
  NyxGtkContainerClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

NYX_GTK_API
GtkWidget * nyx_gtk_lead_container_new (void);

NYX_GTK_API
void nyx_gtk_lead_container_set_leading (NyxGtkLeadContainer *lead_container, gboolean leading);

NYX_GTK_API
gboolean nyx_gtk_lead_container_get_leading (NyxGtkLeadContainer *lead_container);

NYX_GTK_API
void nyx_gtk_lead_container_set_blocked_actions (NyxGtkLeadContainer *lead_container, NyxGtkVideoActionMask actions);

NYX_GTK_API
NyxGtkVideoActionMask nyx_gtk_lead_container_get_blocked_actions (NyxGtkLeadContainer *lead_container);

G_END_DECLS
