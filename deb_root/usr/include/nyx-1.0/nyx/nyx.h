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

#define __NYX_INSIDE__

#include <nyx/nyx-visibility.h>

#include <nyx/nyx-enums.h>
#include <nyx/nyx-version.h>

#include <nyx/nyx-audio-stream.h>
#include <nyx/nyx-basic-functions.h>
#include <nyx/nyx-enhancer-proxy.h>
#include <nyx/nyx-enhancer-proxy-list.h>
#include <nyx/nyx-feature.h>
#include <nyx/nyx-harvest.h>
#include <nyx/nyx-marker.h>
#include <nyx/nyx-media-item.h>
#include <nyx/nyx-player.h>
#include <nyx/nyx-queue.h>
#include <nyx/nyx-stream.h>
#include <nyx/nyx-stream-list.h>
#include <nyx/nyx-subtitle-stream.h>
#include <nyx/nyx-threaded-object.h>
#include <nyx/nyx-timeline.h>
#include <nyx/nyx-utils.h>
#include <nyx/nyx-video-stream.h>

#include <nyx/nyx-extractable.h>
#include <nyx/nyx-playlistable.h>
#include <nyx/nyx-reactable.h>

#include <nyx/nyx-functionalities-availability.h>
#include <nyx/features/nyx-features-availability.h>

#if NYX_HAVE_DISCOVERER
#include <nyx/features/discoverer/nyx-discoverer.h>
#endif
#if NYX_HAVE_MPRIS
#include <nyx/features/mpris/nyx-mpris.h>
#endif
#if NYX_HAVE_SERVER
#include <nyx/features/server/nyx-server.h>
#endif

#undef __NYX_INSIDE__
