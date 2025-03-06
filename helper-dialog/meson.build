executable(
  'helper-dialog',
  [
    'helper-dialog.c',
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfwm-helper-dialog'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gtk,
    libxfce4util,
    libx11,
  ],
  install: true,
  install_dir: helper_path_prefix / 'xfce4' / 'xfwm4',
)
