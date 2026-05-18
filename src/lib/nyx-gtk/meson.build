nyxgtk_dep = dependency('', required: false)
nyxgtk_option = get_option('nyx-gtk')
nyxgtk_api_name = meson.project_name() + '-gtk' + nyx_version_suffix
nyxgtk_resource_prefix = '/com/github/rafostar/Nyx/nyx-gtk'
build_nyxgtk = false

if nyxgtk_option.disabled()
  subdir_done()
endif

nyxgtk_deps = [
  nyx_dep,
  gst_dep,
  gst_nyx_sink_dep,
  gtk4_dep,
  glib_dep,
  gobject_dep,
  libm,
]

foreach dep : nyxgtk_deps
  if not dep.found()
    if nyxgtk_option.enabled()
      error('nyx-gtk option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

subdir('po')

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name() + '-gtk')
config_h.set_quoted('LOCALEDIR', join_paths (prefix, localedir))
config_h.set_quoted('NYX_GTK_RESOURCE_PREFIX', nyxgtk_resource_prefix)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'NYX_API',
  'NYX_GTK',
)

nyxgtk_visibility_header = configure_file(
  input: '../shared/nyx-api-visibility.h.in',
  output: 'nyx-gtk-visibility.h',
  configuration: visibility_conf,
)

version_conf = configuration_data()

version_conf.set(
  'NYX_GTK_VERSION',
  meson.project_version(),
)
version_conf.set(
  'NYX_GTK_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'NYX_GTK_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'NYX_GTK_MICRO_VERSION',
  version_array[2].to_int(),
)

nyxgtk_version_header = configure_file(
  input: 'nyx-gtk-version.h.in',
  output: 'nyx-gtk-version.h',
  configuration: version_conf,
)
nyxgtk_resources = gnome.compile_resources(
  'nyx-gtk-resources',
  'nyx-gtk.gresources.xml',
  c_name: 'nyx_gtk',
)

# Include the generated headers
nyxgtk_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

nyxgtk_headers = [
  'nyx-gtk.h',
  'nyx-gtk-audio.h',
  'nyx-gtk-av.h',
  'nyx-gtk-enums.h',
  'nyx-gtk-billboard.h',
  'nyx-gtk-container.h',
  'nyx-gtk-extra-menu-button.h',
  'nyx-gtk-lead-container.h',
  'nyx-gtk-next-item-button.h',
  'nyx-gtk-previous-item-button.h',
  'nyx-gtk-seek-bar.h',
  'nyx-gtk-simple-controls.h',
  'nyx-gtk-title-header.h',
  'nyx-gtk-title-label.h',
  'nyx-gtk-toggle-fullscreen-button.h',
  'nyx-gtk-toggle-play-button.h',
  'nyx-gtk-utils.h',
  'nyx-gtk-video.h',
  nyxgtk_version_header,
  nyxgtk_visibility_header,
]
nyxgtk_sources = [
  'nyx-gtk-audio.c',
  'nyx-gtk-av.c',
  'nyx-gtk-billboard.c',
  'nyx-gtk-buffering-animation.c',
  'nyx-gtk-buffering-paintable.c',
  'nyx-gtk-container.c',
  'nyx-gtk-extra-menu-button.c',
  'nyx-gtk-lead-container.c',
  'nyx-gtk-limited-layout.c',
  'nyx-gtk-next-item-button.c',
  'nyx-gtk-previous-item-button.c',
  'nyx-gtk-seek-bar.c',
  'nyx-gtk-simple-controls.c',
  'nyx-gtk-status.c',
  'nyx-gtk-stream-check-button.c',
  'nyx-gtk-title-header.c',
  'nyx-gtk-title-label.c',
  'nyx-gtk-toggle-fullscreen-button.c',
  'nyx-gtk-toggle-play-button.c',
  'nyx-gtk-utils.c',
  'nyx-gtk-version.c',
  'nyx-gtk-video.c',
  'nyx-gtk-video-placeholder.c',
  nyxgtk_resources,
]
nyxgtk_c_args = [
  '-DG_LOG_DOMAIN="NyxGtk"',
  '-DNYX_GTK_COMPILATION',
  '-DNYX_DISABLE_DEPRECATION_WARNINGS',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  nyxgtk_c_args += ['-DNYX_GTK_STATIC_COMPILATION']
endif

nyxgtk_headers_dir = join_paths(includedir, nyx_api_name, 'nyx-gtk')

nyxgtk_enums = gnome.mkenums_simple(
  'nyx-gtk-enum-types',
  sources: nyxgtk_headers,
  decorator: 'NYX_GTK_API',
  header_prefix: '#include <nyx-gtk/nyx-gtk-visibility.h>',
  identifier_prefix: 'NyxGtk',
  symbol_prefix: 'nyx_gtk',
  install_header: true,
  install_dir: nyxgtk_headers_dir,
)

nyxgtk_lib = library(
  nyxgtk_api_name,
  nyxgtk_sources + nyxgtk_enums,
  dependencies: nyxgtk_deps,
  include_directories: nyxgtk_conf_inc,
  c_args: nyxgtk_c_args,
  version: nyx_version,
  install: true,
)
install_headers(nyxgtk_headers,
  install_dir: nyxgtk_headers_dir,
)
build_nyxgtk = true

if build_gir
  nyxgtk_gir = gnome.generate_gir(nyxgtk_lib,
    sources: [
      nyxgtk_sources,
      nyxgtk_headers,
      nyxgtk_enums,
    ],
    extra_args: [
      '--quiet',
      '--warn-all',
      '-DNYX_GTK_COMPILATION',
      '-DGST_USE_UNSTABLE_API',
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'NyxGtk',
    identifier_prefix: 'NyxGtk',
    symbol_prefix: 'nyx_gtk',
    export_packages: nyxgtk_api_name,
    install: true,
    includes: [
      nyx_gir[0],
      'Gst-1.0',
      'Gtk-4.0',
      'GLib-2.0',
      'GObject-2.0',
    ],
    header: join_paths(meson.project_name() + '-gtk', 'nyx-gtk.h'),
  )
endif

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    gnome.generate_vapi(nyxgtk_api_name,
      sources: nyxgtk_gir[0],
      packages: nyx_pkg_reqs + [
        nyx_vapi,
        'gtk4',
      ],
      install: true,
    )
  endif
endif

pkgconfig.generate(nyxgtk_lib,
  subdirs: [nyxgtk_api_name],
  filebase: nyxgtk_api_name,
  name: meson.project_name() + '-gtk',
  version: meson.project_version(),
  description: 'Nyx GTK integration library',
  requires: [
    nyx_lib,
    'gstreamer-1.0',
    'gtk4',
    'glib-2.0',
    'gobject-2.0',
  ],
)

nyxgtk_dep = declare_dependency(
  link_with: nyxgtk_lib,
  include_directories: nyxgtk_conf_inc,
  dependencies: nyxgtk_deps,
  sources: [
    nyxgtk_visibility_header,
    nyxgtk_version_header,
    nyxgtk_enums[1],
  ],
)
