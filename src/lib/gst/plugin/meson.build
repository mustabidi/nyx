gst_nyx_sink_dep = dependency('', required: false)

gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')
gst_nyx_importers_libdir = join_paths(nyx_libdir, 'gst', 'plugin', 'importers')

gst_nyx_plugin_deps = [
  gtk4_dep,
  gst_dep,
  gst_base_dep,
  gst_video_dep,
  gmodule_dep,
]

build_gst_plugin = not get_option('gst-plugin').disabled()
foreach dep : gst_nyx_plugin_deps
  if not dep.found()
    if get_option('gst-plugin').enabled()
      error('GStreamer plugin was enabled, but required dependencies were not found')
    endif
    build_gst_plugin = false
  endif
endforeach

gst_nyx_plugin_args = [
  '-DHAVE_CONFIG_H',
  '-DNYX_GST_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  gst_nyx_plugin_args += [
    '-DGST_STATIC_COMPILATION',
    '-DNYX_GST_STATIC_COMPILATION',
  ]
endif

cdata = configuration_data()

cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())

cdata.set_quoted('GST_PACKAGE_NAME', 'gst-plugin-nyx')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/Rafostar/nyx')
cdata.set_quoted('GST_LICENSE', 'LGPL')

cdata.set_quoted('NYX_SINK_IMPORTER_PATH', gst_nyx_importers_libdir)

configure_file(
  output: 'config.h',
  configuration: cdata,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'NYX_API',
  'NYX_GST',
)

nyxgst_visibility_header = configure_file(
  input: '../../shared/nyx-api-visibility.h.in',
  output: 'nyx-gst-visibility.h',
  configuration: visibility_conf,
)

gst_plugin_conf_inc = [
  include_directories('.'),
  include_directories('..'),
  include_directories('../..'),
]

gst_nyx_plugin_sources = [
  'gstnyxsink.c',
  'gstnyxpaintable.c',
  'gstgtkutils.c',
  'gstplugin.c',
  'gstnyxcontexthandler.c',
  'gstnyximporter.c',
  'gstnyximporterloader.c',
]

if build_gst_plugin
  gst_nyx_sink_dep = declare_dependency(
    link_with: library('gstnyx',
      gst_nyx_plugin_sources,
      c_args: gst_nyx_plugin_args,
      include_directories: gst_plugin_conf_inc,
      dependencies: gst_nyx_plugin_deps,
      install: true,
      install_dir: gst_plugins_libdir,
    ),
    include_directories: gst_plugin_conf_inc,
    dependencies: gst_nyx_plugin_deps,
  )
endif

subdir('handlers')
subdir('importers')
