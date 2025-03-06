libxfwm_common = static_library(
  'xfwm-common',
  [
    'xfwm-common.c',
    'xfwm-common.h',
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libxfwm-common'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gtk,
  ],
  install: false,
)
