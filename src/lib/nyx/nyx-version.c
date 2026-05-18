/* Nyx Playback Library
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

#include "nyx-version.h"

/**
 * nyx_get_major_version:
 *
 * Nyx runtime major version component
 *
 * This returns the Nyx library version your code is
 * running against unlike [const@Nyx.MAJOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the major version number of the Nyx library
 *
 * Since: 0.10
 */
guint
nyx_get_major_version (void)
{
  return NYX_MAJOR_VERSION;
}

/**
 * nyx_get_minor_version:
 *
 * Nyx runtime minor version component
 *
 * This returns the Nyx library version your code is
 * running against unlike [const@Nyx.MINOR_VERSION]
 * which represents compile time version.
 *
 * Returns: the minor version number of the Nyx library
 *
 * Since: 0.10
 */
guint
nyx_get_minor_version (void)
{
  return NYX_MINOR_VERSION;
}

/**
 * nyx_get_micro_version:
 *
 * Nyx runtime micro version component
 *
 * This returns the Nyx library version your code is
 * running against unlike [const@Nyx.MICRO_VERSION]
 * which represents compile time version.
 *
 * Returns: the micro version number of the Nyx library
 *
 * Since: 0.10
 */
guint
nyx_get_micro_version (void)
{
  return NYX_MICRO_VERSION;
}

/**
 * nyx_get_version_s:
 *
 * Nyx runtime version as string
 *
 * This returns the Nyx library version your code is
 * running against unlike [const@Nyx.VERSION_S]
 * which represents compile time version.
 *
 * Returns: the version of the Nyx library as string
 *
 * Since: 0.10
 */
const gchar *
nyx_get_version_s (void)
{
  return NYX_VERSION_S;
}
