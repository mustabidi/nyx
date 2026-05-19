nyxapp_option = get_option('nyx-app')
app_id = 'io.github.moon.Nyx'
app_resource_prefix = '/io/github/moon/Nyx/nyx-app'
build_nyxapp = false

if nyxapp_option.disabled()
  subdir_done()
endif

nyxapp_deps = [
  nyx_dep,
  nyxgtk_dep,
  gst_dep,
  gtk4_dep,
  libadwaita_dep,
  glib_dep,
  gobject_dep,
]

foreach dep : nyxapp_deps
  if not dep.found()
    if nyxapp_option.enabled()
      error('nyx-app option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

subdir('data')
subdir('po')

nyxapp_resources = gnome.compile_resources(
  'nyx-app-resources',
  'nyx-app.gresources.xml',
  c_name: 'nyx_app',
)

# Include the generated headers
nyxapp_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', 'nyx-app')
config_h.set_quoted('LOCALEDIR', join_paths (prefix, localedir))
config_h.set_quoted('NYX_APP_NAME', 'Nyx')
config_h.set_quoted('NYX_APP_ID', app_id)
config_h.set_quoted('NYX_APP_RESOURCE_PREFIX', app_resource_prefix)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'NYX_API',
  'NYX_APP_INTERNAL',
)

configure_file(
  input: '../../lib/shared/nyx-api-visibility.h.in',
  output: 'nyx-app-internal-visibility.h',
  configuration: visibility_conf,
)

nyxapp_sources = [
  'nyx-app-about-dialog.c',
  'nyx-app-application.c',
  'nyx-app-dir-scanner.c',
  'nyx-app-file-dialog.c',
  'nyx-app-headerbar.c',
  'nyx-app-info-window.c',
  'nyx-app-list-item-utils.c',
  'nyx-app-media-item-box.c',
  'nyx-app-preferences-window.c',
  'nyx-app-property-row.c',
  'nyx-app-queue-list.c',
  'nyx-app-queue-progression-item.c',
  'nyx-app-queue-progression-model.c',
  'nyx-app-queue-selection.c',
  'nyx-app-sub-finder.c',
  'nyx-app-types.c',
  'nyx-app-uri-dialog.c',
  'nyx-app-utils.c',
  'nyx-app-window.c',
  'nyx-app-window-state-buttons.c',
  'main.c',
  nyxapp_resources,
]
nyxapp_c_args = [
  '-DG_LOG_DOMAIN="NyxApp"',
  '-DNYX_APP_INTERNAL_COMPILATION',
  '-DNYX_DISABLE_DEPRECATION_WARNINGS',
  '-DGST_USE_UNSTABLE_API',
]

nyxapp_possible_functionalities = [
  'pipeline-preview',
]
nyxapp_available_functionalities = []

pp_option = get_option('pipeline-preview')

if not pp_option.disabled()
  if cgraph_dep.found() and gvc_dep.found()
    nyxapp_c_args += ['-DHAVE_GRAPHVIZ']
    nyxapp_deps += [cgraph_dep, gvc_dep]
    if gvc_dep.version().version_compare('>= 13.0.0')
      nyxapp_c_args += ['-DHAVE_GVC_13']
    endif
    nyxapp_available_functionalities += 'pipeline-preview'
  elif pp_option.enabled()
    error('pipeline-preview option was enabled, but required dependencies were not found')
  endif
endif

is_windows = ['windows'].contains(host_machine.system())

if is_windows
  nyxapp_c_args += ['-D_WIN32_WINNT=_WIN32_WINNT_WIN8']
  kernel32_dep = cc.find_library('kernel32', required: false)
  if kernel32_dep.found() and cc.has_header('processthreadsapi.h')
    nyxapp_deps += kernel32_dep
    nyxapp_c_args += ['-DHAVE_WIN_PROCESS_THREADS_API']
  endif
  winmm_dep = cc.find_library('winmm', required: false)
  if winmm_dep.found() and cc.has_header('timeapi.h')
    nyxapp_deps += winmm_dep
    nyxapp_c_args += ['-DHAVE_WIN_TIME_API']
  endif
  subdir('windows')
endif

executable(
  meson.project_name(),
  nyxapp_sources,
  dependencies: nyxapp_deps,
  include_directories: nyxapp_conf_inc,
  c_args: nyxapp_c_args,
  install: true,
  install_dir: bindir,
  win_subsystem: 'windows',
)
if is_windows
  executable(
    meson.project_name() + '-console',
    nyxapp_sources,
    dependencies: nyxapp_deps,
    include_directories: nyxapp_conf_inc,
    c_args: nyxapp_c_args,
    install: true,
    install_dir: bindir,
    win_subsystem: 'console',
  )
endif
build_nyxapp = true
