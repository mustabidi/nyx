nyx_dep = dependency('', required: false)
nyx_option = get_option('nyx')
nyx_headers_dir = join_paths(includedir, nyx_api_name, 'nyx')
nyx_enhancers_dir = join_paths(nyx_libdir, 'enhancers')
nyx_with_enhancers_loader = false
build_nyx = false

nyx_pkg_reqs = [
  'gstreamer-1.0',
  'gstreamer-base-1.0',
  'gstreamer-audio-1.0',
  'gstreamer-tag-1.0',
  'gstreamer-pbutils-1.0',
  'glib-2.0',
  'gobject-2.0',
  'gio-2.0',
]

if nyx_option.disabled()
  subdir_done()
endif

nyx_deps = [
  gst_dep,
  gst_base_dep,
  gst_audio_dep,
  gst_tag_dep,
  gst_pbutils_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
]

foreach dep : nyx_deps
  if not dep.found()
    if nyx_option.enabled()
      error('nyx option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

# libpeas is an optional dependency
enhancers_option = get_option('enhancers-loader')
nyx_with_enhancers_loader = (not enhancers_option.disabled() and peas_dep.found())

if not nyx_with_enhancers_loader and enhancers_option.enabled()
  error('enhancers-loader option was enabled, but required dependencies were not found')
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_ORIGIN', 'https://github.com/Rafostar/nyx')
config_h.set_quoted('PLUGIN_DESC', 'Nyx elements')
config_h.set_quoted('PLUGIN_LICENSE', 'LGPL')
config_h.set_quoted('NYX_API_NAME', nyx_api_name)
config_h.set_quoted('NYX_ENHANCERS_PATH', nyx_enhancers_dir)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'NYX_API',
  'NYX',
)

nyx_visibility_header = configure_file(
  input: '../shared/nyx-api-visibility.h.in',
  output: 'nyx-visibility.h',
  configuration: visibility_conf,
)

version_conf = configuration_data()

version_conf.set(
  'NYX_VERSION',
  meson.project_version(),
)
version_conf.set(
  'NYX_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'NYX_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'NYX_MICRO_VERSION',
  version_array[2].to_int(),
)

nyx_version_header = configure_file(
  input: 'nyx-version.h.in',
  output: 'nyx-version.h',
  configuration: version_conf,
)

# Include the generated headers
nyx_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

nyx_headers = [
  'nyx.h',
  'nyx-enums.h',
  'nyx-audio-stream.h',
  'nyx-basic-functions.h',
  'nyx-enhancer-proxy.h',
  'nyx-enhancer-proxy-list.h',
  'nyx-extractable.h',
  'nyx-feature.h',
  'nyx-harvest.h',
  'nyx-marker.h',
  'nyx-media-item.h',
  'nyx-player.h',
  'nyx-playlistable.h',
  'nyx-queue.h',
  'nyx-reactable.h',
  'nyx-stream.h',
  'nyx-stream-list.h',
  'nyx-subtitle-stream.h',
  'nyx-threaded-object.h',
  'nyx-timeline.h',
  'nyx-utils.h',
  'nyx-video-stream.h',
  nyx_version_header,
  nyx_visibility_header,
]
nyx_sources = [
  'nyx-app-bus.c',
  'nyx-audio-stream.c',
  'nyx-basic-functions.c',
  'nyx-cache.c',
  'nyx-enhancer-proxy.c',
  'nyx-enhancer-proxy-list.c',
  'nyx-extractable.c',
  'nyx-feature.c',
  'nyx-features-bus.c',
  'nyx-features-manager.c',
  'nyx-harvest.c',
  'nyx-marker.c',
  'nyx-media-item.c',
  'nyx-playbin-bus.c',
  'nyx-player.c',
  'nyx-playlistable.c',
  'nyx-queue.c',
  'nyx-reactable.c',
  'nyx-reactables-manager.c',
  'nyx-stream.c',
  'nyx-stream-list.c',
  'nyx-subtitle-stream.c',
  'nyx-threaded-object.c',
  'nyx-timeline.c',
  'nyx-utils.c',
  'nyx-version.c',
  'nyx-video-stream.c',
  'gst/nyx-plugin.c',
  'gst/nyx-extractable-src.c',
  'gst/nyx-enhancer-director.c',
  'gst/nyx-uri-base-demux.c',
  'gst/nyx-harvest-uri-demux.c',
  'gst/nyx-playlist-demux.c',
  '../shared/nyx-shared-utils.c',
]
nyx_c_args = [
  '-DG_LOG_DOMAIN="Nyx"',
  '-DNYX_COMPILATION',
  '-DNYX_DISABLE_DEPRECATION_WARNINGS',
  '-DGST_USE_UNSTABLE_API',
  '-DG_SETTINGS_ENABLE_BACKEND',
]

if get_option('default_library') == 'static'
  nyx_c_args += ['-DNYX_STATIC_COMPILATION']
endif

nyx_possible_functionalities = [
  'enhancers-loader',
]
nyx_available_functionalities = []

if nyx_with_enhancers_loader
  nyx_deps += peas_dep
  nyx_sources += [
    'nyx-enhancers-loader.c',
  ]
  nyx_available_functionalities += 'enhancers-loader'
endif

functionalities_availability_conf = configuration_data()

foreach functionality_name : nyx_possible_functionalities
  functionalities_availability_conf.set(
    'NYX_WITH_@0@'.format(functionality_name.replace('-', '_').to_upper()),
    nyx_available_functionalities.contains(functionality_name) ? 'TRUE' : 'FALSE'
  )
endforeach

nyx_headers += configure_file(
  input: 'nyx-functionalities-availability.h.in',
  output: 'nyx-functionalities-availability.h',
  configuration: functionalities_availability_conf,
)

subdir('features')

nyx_enums = gnome.mkenums_simple(
  'nyx-enum-types',
  sources: nyx_headers,
  decorator: 'NYX_API',
  header_prefix: '#include <nyx/nyx-visibility.h>',
  identifier_prefix: 'Nyx',
  symbol_prefix: 'nyx',
  install_header: true,
  install_dir: nyx_headers_dir,
)

nyx_lib = library(
  nyx_api_name,
  nyx_sources + nyx_features_sources + nyx_features_sources_internal + nyx_enums,
  dependencies: nyx_deps + nyx_features_deps,
  include_directories: nyx_conf_inc,
  c_args: nyx_c_args,
  version: nyx_version,
  install: true,
)
install_headers(nyx_headers,
  install_dir: nyx_headers_dir,
)
build_nyx = true

if build_gir
  nyx_gir = gnome.generate_gir(nyx_lib,
    sources: [
      nyx_sources,
      nyx_features_sources,
      nyx_headers,
      nyx_features_headers,
      nyx_enums,
    ],
    extra_args: [
      gir_init_section,
      '--quiet',
      '--warn-all',
      '-DNYX_COMPILATION',
      '-DGST_USE_UNSTABLE_API',
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'Nyx',
    identifier_prefix: 'Nyx',
    symbol_prefix: 'nyx',
    export_packages: nyx_api_name,
    install: true,
    includes: [
      'Gst-1.0',
      'GstBase-1.0',
      'GstAudio-1.0',
      'GstTag-1.0',
      'GstPbutils-1.0',
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
    ],
    header: join_paths(meson.project_name(), 'nyx.h'),
  )
endif

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    nyx_vapi = gnome.generate_vapi(nyx_api_name,
      sources: nyx_gir[0],
      packages: nyx_pkg_reqs,
      metadata_dirs: [
        join_paths(meson.current_source_dir(), 'metadata')
      ],
      install: true,
    )
  endif
endif

nyx_pkgconfig_variables = [
  'enhancersdir=${libdir}/' + nyx_api_name + '/enhancers',
  'features=' + ' '.join(nyx_available_features),
  'functionalities=' + ' '.join(nyx_available_functionalities),
]

pkgconfig.generate(nyx_lib,
  unescaped_variables: nyx_pkgconfig_variables,
  subdirs: [nyx_api_name],
  filebase: nyx_api_name,
  name: meson.project_name(),
  version: meson.project_version(),
  description: 'Nyx playback library',
  requires: nyx_pkg_reqs,
)

nyx_dep = declare_dependency(
  link_with: nyx_lib,
  include_directories: nyx_conf_inc,
  dependencies: nyx_deps + nyx_features_deps,
  sources: [
    nyx_visibility_header,
    nyx_version_header,
    nyx_features_availability_header,
    nyx_enums[1],
  ],
)
