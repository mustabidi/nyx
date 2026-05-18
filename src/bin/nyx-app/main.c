/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <nyx/nyx.h>

#include "nyx-app-application.h"
#include "nyx-app-types.h"
#include "nyx-app-utils.h"

gint
main (gint argc, gchar **argv)
{
  GApplication *application;
  NyxEnhancerProxyList *proxies;
  const gchar *nyx_ldir;
  guint i, n_proxies;
  gint status;

#ifdef G_OS_WIN32
  guint resolution = 0;
#endif

#ifndef G_OS_WIN32
  g_setenv ("GSK_RENDERER", "gl", FALSE);
#endif

  setlocale (LC_ALL, "");
  if (!(nyx_ldir = g_getenv ("NYX_APP_OVERRIDE_LOCALEDIR")))
    nyx_ldir = LOCALEDIR;
  bindtextdomain (GETTEXT_PACKAGE, nyx_ldir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  nyx_init (&argc, &argv);
  gtk_init ();
  adw_init ();

  nyx_app_types_init ();
  nyx_app_utils_debug_init ();

  g_set_application_name ("Nyx");

#ifdef G_OS_WIN32
  nyx_app_utils_win_enforce_hi_res_clock ();
  resolution = nyx_app_utils_win_hi_res_clock_start ();
#endif

  proxies = nyx_get_global_enhancer_proxies ();
  n_proxies = nyx_enhancer_proxy_list_get_n_proxies (proxies);

  /* Allow usage of all enhancers */
  for (i = 0; i < n_proxies; ++i) {
    NyxEnhancerProxy *proxy = nyx_enhancer_proxy_list_peek_proxy (proxies, i);
    nyx_enhancer_proxy_set_target_creation_allowed (proxy, TRUE);
  }

  application = nyx_app_application_new ();

  status = g_application_run (application, argc, argv);
  g_object_unref (application);

#ifdef G_OS_WIN32
  if (resolution > 0)
    nyx_app_utils_win_hi_res_clock_stop (resolution);
#endif

  nyx_app_utils_delete_tmp_dir ();

  return status;
}
