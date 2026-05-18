/* Nyx GTK Integration Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "nyx-gtk-version.h"

/**
 * nyx_gtk_get_major_version:
 *
 * NyxGtk runtime major version component
 *
 * This returns the NyxGtk library version your code is
 * running against unlike [const@NyxGtk.MAJOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the major version number of the NyxGtk library
 *
 * Since: 0.10
 */
guint
nyx_gtk_get_major_version (void)
{
  return NYX_GTK_MAJOR_VERSION;
}

/**
 * nyx_gtk_get_minor_version:
 *
 * NyxGtk runtime minor version component
 *
 * This returns the NyxGtk library version your code is
 * running against unlike [const@NyxGtk.MINOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the minor version number of the NyxGtk library
 *
 * Since: 0.10
 */
guint
nyx_gtk_get_minor_version (void)
{
  return NYX_GTK_MINOR_VERSION;
}

/**
 * nyx_gtk_get_micro_version:
 *
 * NyxGtk runtime micro version component
 *
 * This returns the NyxGtk library version your code is
 * running against unlike [const@NyxGtk.MICRO_VERSION]
 * which represents compile time version.
 *
 * Returns: the micro version number of the NyxGtk library
 *
 * Since: 0.10
 */
guint
nyx_gtk_get_micro_version (void)
{
  return NYX_GTK_MICRO_VERSION;
}

/**
 * nyx_gtk_get_version_s:
 *
 * NyxGtk runtime version as string
 *
 * This returns the NyxGtk library version your code is
 * running against unlike [const@NyxGtk.VERSION_S]
 * which represents compile time version.
 *
 * Returns: the version of the NyxGtk library as string
 *
 * Since: 0.10
 */
const gchar *
nyx_gtk_get_version_s (void)
{
  return NYX_GTK_VERSION_S;
}
