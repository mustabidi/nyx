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

#include "config.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "nyx-basic-functions.h"
#include "nyx-cache-private.h"
#include "nyx-utils-private.h"
#include "nyx-playbin-bus-private.h"
#include "nyx-app-bus-private.h"
#include "nyx-features-bus-private.h"
#include "nyx-enhancer-proxy-list-private.h"
#include "nyx-reactables-manager-private.h"
#include "gst/nyx-plugin-private.h"

#include "nyx-functionalities-availability.h"

#if NYX_WITH_ENHANCERS_LOADER
#include "nyx-enhancers-loader-private.h"
#endif

static NyxEnhancerProxyList *_proxies = NULL;
static gboolean is_initialized = FALSE;
static GMutex init_lock;

static gboolean
nyx_init_check_internal (int *argc, char **argv[])
{
  g_mutex_lock (&init_lock);

  if (is_initialized || !gst_init_check (argc, argv, NULL))
    goto finish;

  gst_pb_utils_init ();

  nyx_cache_initialize ();
  nyx_utils_initialize ();
  nyx_playbin_bus_initialize ();
  nyx_app_bus_initialize ();
  nyx_features_bus_initialize ();
  nyx_reactables_manager_initialize ();

  _proxies = nyx_enhancer_proxy_list_new_named ("global-proxy-list");

#if NYX_WITH_ENHANCERS_LOADER
  nyx_enhancers_loader_initialize (_proxies);
#endif

  gst_plugin_register_static (
      GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      PACKAGE "internal",
      PLUGIN_DESC,
      (GstPluginInitFunc) nyx_gst_plugin_init,
      PACKAGE_VERSION,
      PLUGIN_LICENSE,
      PACKAGE,
      PACKAGE,
      PACKAGE_ORIGIN);

  is_initialized = TRUE;

finish:
  g_mutex_unlock (&init_lock);

  return is_initialized;
}

/**
 * nyx_init:
 * @argc: (inout) (nullable) (optional): pointer to application's argc
 * @argv: (inout) (array length=argc) (nullable) (optional): pointer to application's argv
 *
 * Initializes the Nyx library. Implementations must always call this
 * before using Nyx API.
 *
 * Because Nyx uses GStreamer internally, this function will also initialize
 * GStreamer before initializing Nyx itself for user convienience, so
 * application does not have to do so anymore.
 *
 * WARNING: This function will terminate your program if it was unable to
 * initialize for some reason. If you want to do some fallback logic,
 * use [func@Nyx.init_check] instead.
 */
void
nyx_init (int *argc, char **argv[])
{
  if (!nyx_init_check_internal (argc, argv)) {
    g_printerr ("Could not initialize Nyx library\n");
    exit (1);
  }
}

/**
 * nyx_init_check:
 * @argc: (inout) (nullable) (optional): pointer to application's argc
 * @argv: (inout) (array length=argc) (nullable) (optional): pointer to application's argv
 *
 * This function does the same thing as [func@Nyx.init], but instead of
 * terminating on failure it returns %FALSE.
 *
 * Returns: %TRUE if Nyx could be initialized, %FALSE otherwise.
 */
gboolean
nyx_init_check (int *argc, char **argv[])
{
  return nyx_init_check_internal (argc, argv);
}

/**
 * nyx_enhancer_check:
 * @iface_type: an interface #GType
 * @scheme: an URI scheme
 * @host: (nullable): an URI host
 * @name: (out) (optional) (transfer none): return location for found enhancer name
 *
 * Check if an enhancer of @type is available for given @scheme and @host.
 *
 * A check that compares requested capabilites of all available Nyx enhancers,
 * thus it is fast but does not guarantee that the found one will succeed. Please note
 * that this function will always return %FALSE if Nyx was built without enhancers
 * loader functionality. To check that, use [const@Nyx.WITH_ENHANCERS_LOADER].
 *
 * This function can be used to quickly determine early if Nyx will at least try to
 * handle URI and with one of its enhancers and which one.
 *
 * Example:
 *
 * ```c
 * gboolean supported = nyx_enhancer_check (NYX_TYPE_EXTRACTABLE, "https", "example.com", NULL);
 * ```
 *
 * For self hosted services a custom URI @scheme without @host can be used. Enhancers should announce
 * support for such schemes by defining them in their plugin info files.
 *
 * ```c
 * gboolean supported = nyx_enhancer_check (NYX_TYPE_EXTRACTABLE, "example", NULL, NULL);
 * ```
 *
 * Returns: whether a plausible enhancer was found.
 *
 * Since: 0.8
 *
 * Deprecated: 0.10: Use list of enhancer proxies from [func@Nyx.get_global_enhancer_proxies] or
 *   [property@Nyx.Player:enhancer-proxies] and check if any proxy matches your search criteria.
 */
gboolean
nyx_enhancer_check (GType iface_type, const gchar *scheme, const gchar *host, const gchar **name)
{
  gboolean is_https;
  guint i, n_proxies;

  g_return_val_if_fail (G_TYPE_IS_INTERFACE (iface_type), FALSE);
  g_return_val_if_fail (scheme != NULL, FALSE);

  if (host) {
    /* Strip common subdomains, so plugins do not
     * have to list all combinations */
    if (g_str_has_prefix (host, "www."))
      host += 4;
    else if (g_str_has_prefix (host, "m."))
      host += 2;
  }

  /* Whether "http(s)" scheme is used */
  is_https = (g_str_has_prefix (scheme, "http")
      && (scheme[4] == '\0' || (scheme[4] == 's' && scheme[5] == '\0')));

  if (!host && is_https)
    return FALSE;

  n_proxies = nyx_enhancer_proxy_list_get_n_proxies (_proxies);
  for (i = 0; i < n_proxies; ++i) {
    NyxEnhancerProxy *proxy = nyx_enhancer_proxy_list_peek_proxy (_proxies, i);

    if (nyx_enhancer_proxy_target_has_interface (proxy, iface_type)
        && nyx_enhancer_proxy_extra_data_lists_value (proxy, "X-Schemes", scheme)
        && (!is_https || nyx_enhancer_proxy_extra_data_lists_value (proxy, "X-Hosts", host))) {
      if (name)
        *name = nyx_enhancer_proxy_get_friendly_name (proxy);

      return TRUE;
    }
  }

  return FALSE;
}

/**
 * nyx_get_global_enhancer_proxies:
 *
 * Get a list of available enhancers in the form of [class@Nyx.EnhancerProxy] objects.
 *
 * This returns a global list of enhancer proxy objects. You can use it to inspect
 * available enhancers without creating a new player instance.
 *
 * Remember to initialize Nyx library before using this function.
 *
 * Only enhancer properties with [flags@Nyx.EnhancerParamFlags.GLOBAL] flag can be
 * set on proxies in this list. These are meant to be set ONLY by users, not applications
 * as they carry over to all player instances (possibly including other apps). Applications
 * should instead be changing properties with [flags@Nyx.EnhancerParamFlags.LOCAL] flag
 * set from individual proxy lists from [property@Nyx.Player:enhancer-proxies] which
 * will affect only that single player instance given list belongs to.
 *
 * Returns: (transfer none): a global #NyxEnhancerProxyList of enhancer proxies.
 *
 * Since: 0.10
 */
NyxEnhancerProxyList *
nyx_get_global_enhancer_proxies (void)
{
  return _proxies;
}
