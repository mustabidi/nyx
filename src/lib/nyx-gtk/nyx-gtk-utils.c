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

#include "config.h"

#include <glib/gi18n-lib.h>

#include "nyx-gtk-utils-private.h"
#include "nyx-gtk-av.h"

static gboolean initialized = FALSE;

/**
 * nyx_gtk_get_player_from_ancestor:
 * @widget: a #GtkWidget
 *
 * Get [class@Nyx.Player] used by [class@NyxGtk.Av] ancestor of @widget.
 *
 * This utility is a convenience wrapper for calling [method@Gtk.Widget.get_ancestor]
 * of type `NYX_GTK_TYPE_AV` and [method@NyxGtk.Av.get_player] with
 * additional %NULL checking and type casting.
 *
 * This is meant to be used mainly for custom widget development as an easy access to the
 * underlying parent [class@Nyx.Player] object. If you want to get the player from
 * [class@NyxGtk.Av] widget itself, use [method@NyxGtk.Av.get_player] instead.
 *
 * Rememeber that this function will return %NULL when widget does not have
 * a [class@NyxGtk.Av] ancestor in widget hierarchy (widget is not yet placed).
 *
 * Returns: (transfer none) (nullable): a #NyxPlayer from ancestor of a @widget.
 */
NyxPlayer *
nyx_gtk_get_player_from_ancestor (GtkWidget *widget)
{
  GtkWidget *parent;
  NyxPlayer *player = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if ((parent = gtk_widget_get_ancestor (widget, NYX_GTK_TYPE_AV)))
    player = nyx_gtk_av_get_player (NYX_GTK_AV_CAST (parent));

  return player;
}

void
nyx_gtk_init_translations (void)
{
  const gchar *nyx_gtk_ldir;

  if (initialized)
    return;

  if (!(nyx_gtk_ldir = g_getenv ("NYX_GTK_OVERRIDE_LOCALEDIR")))
    nyx_gtk_ldir = LOCALEDIR;
  bindtextdomain (GETTEXT_PACKAGE, nyx_gtk_ldir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  initialized = TRUE;
}

const gchar *
nyx_gtk_get_icon_name_for_volume (gfloat volume)
{
  return (volume <= 0.0f)
      ? "audio-volume-muted-symbolic"
      : (volume <= 0.3f)
      ? "audio-volume-low-symbolic"
      : (volume <= 0.7f)
      ? "audio-volume-medium-symbolic"
      : (volume <= 1.0f)
      ? "audio-volume-high-symbolic"
      : "audio-volume-overamplified-symbolic";
}

const gchar *
nyx_gtk_get_icon_name_for_speed (gfloat speed)
{
  return (speed < 1.0f)
      ? "nyx-gtk-speed-slow-symbolic"
      : (speed == 1.0f)
      ? "nyx-gtk-speed-normal-symbolic"
      : "nyx-gtk-speed-fast-symbolic";
}
