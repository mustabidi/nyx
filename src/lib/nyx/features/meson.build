nyx_features_headers = []
nyx_features_sources = []
nyx_features_sources_internal = []
nyx_features_deps = []
nyx_available_features = []

features_availability_conf = configuration_data()

nyx_possible_features = [
  'discoverer',
  'mpris',
  'server',
]
foreach feature_name : nyx_possible_features
  subdir(feature_name)
  features_availability_conf.set(
    'NYX_HAVE_@0@'.format(feature_name.replace('-', '_').to_upper()),
    nyx_available_features.contains(feature_name) ? 'TRUE' : 'FALSE'
  )
endforeach

nyx_features_availability_header = configure_file(
  input: 'nyx-features-availability.h.in',
  output: 'nyx-features-availability.h',
  configuration: features_availability_conf,
)
install_headers(nyx_features_availability_header,
  install_dir: join_paths(nyx_headers_dir, 'features'),
)
nyx_features_headers += [
  nyx_features_availability_header
]
