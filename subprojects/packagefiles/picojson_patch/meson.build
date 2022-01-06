project('picojson', 'cpp')

picojson_dep = declare_dependency(include_directories : include_directories('.'))
if meson.version().version_compare('>=0.54.0')
  meson.override_dependency('picojson', picojson_dep)
endif