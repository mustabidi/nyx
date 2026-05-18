feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

# Known OSes that can support our MPRIS implementation
os_supported = ['linux'].contains(host_machine.system())

if not os_supported
  if feature_option.enabled()
    error('@0@ feature was enabled, but OS is not supported by it'.format(feature_name))
  endif
  subdir_done()
endif

feature_deps = [
  dependency('gio-unix-2.0', version: glib_req, required: false),
]
foreach dep : feature_deps
  if not dep.found()
    if feature_option.enabled()
      error('@0@ feature was enabled, but required dependencies were not found'.format(feature_name))
    endif
    subdir_done()
  endif
endforeach

gdbus_codegen = find_program('gdbus-codegen', required: false)
if not gdbus_codegen.found()
  if feature_option.enabled()
    error('@0@ feature was enabled, but program gdbus-codegen was not found'.format(feature_name))
  endif
  subdir_done()
endif

nyx_mpris_gdbus = gnome.gdbus_codegen('nyx-mpris-gdbus',
  sources: 'nyx-mpris-gdbus.xml',
  interface_prefix: 'org.mpris.',
  namespace: 'NyxMpris',
)
nyx_features_headers += [
  'features/mpris/nyx-mpris.h',
]
nyx_features_sources += [
  'features/mpris/nyx-mpris.c',
]
nyx_features_sources_internal += [
  nyx_mpris_gdbus,
]
nyx_features_deps += feature_deps

install_headers('nyx-mpris.h',
  install_dir: join_paths(nyx_headers_dir, 'features', 'mpris'),
)
nyx_available_features += feature_name
