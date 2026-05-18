feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

nyx_features_headers += [
  'features/discoverer/nyx-discoverer.h',
]
nyx_features_sources += [
  'features/discoverer/nyx-discoverer.c',
]

install_headers('nyx-discoverer.h',
  install_dir: join_paths(nyx_headers_dir, 'features', 'discoverer'),
)
nyx_available_features += feature_name
