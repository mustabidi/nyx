feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

feature_deps = [
  dependency('libsoup-3.0', required: false),
  dependency('microdns', version: '>= 0.2.0', required: false),
]
foreach dep : feature_deps
  if not dep.found()
    if feature_option.enabled()
      error('@0@ feature was enabled, but required dependencies were not found'.format(feature_name))
    endif
    subdir_done()
  endif
endforeach

nyx_features_headers += [
  'features/server/nyx-server.h',
]
nyx_features_sources += [
  'features/server/nyx-server.c',
  'features/server/nyx-server-json.c',
  'features/server/nyx-server-actions.c',
  'features/server/nyx-server-mdns.c',
]
nyx_features_deps += feature_deps

install_headers('nyx-server.h',
  install_dir: join_paths(nyx_headers_dir, 'features', 'server'),
)
nyx_available_features += feature_name
