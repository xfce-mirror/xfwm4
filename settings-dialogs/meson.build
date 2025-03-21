xfwm4_resources_c = gnome.compile_resources(
  'xfwm4-resources',
  'xfwm4.gresource.xml',
)

main_dialog_sources = [
  'range-debouncer.c',
  'range-debouncer.h',
  'xfwm4-settings.c',
  'xfwm4-settings.h',
]

executable(
  'xfwm4-settings',
  main_dialog_sources,
  sources: xfwm4_resources_c,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfwm4-settings'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4kbd,
    libxfce4util,
    xfconf,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

tweaks_dialog_sources = [
  'range-debouncer.c',
  'range-debouncer.h',
  'tweaks-settings.c',
]

executable(
  'xfwm4-tweaks-settings',
  tweaks_dialog_sources,
  sources: xfwm4_resources_c,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfwm4-tweaks-settings'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

workspaces_dialog_sources = [
  'workspace-settings.c',
]

executable(
  'xfwm4-workspace-settings',
  workspaces_dialog_sources,
  sources: xfwm4_resources_c,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfwm4-workspace-settings'),
    '-DWNCK_I_KNOW_THIS_IS_UNSTABLE=1',
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
    libwnck,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

desktop_files = [
  'xfce-wm-settings.desktop.in',
  'xfce-wmtweaks-settings.desktop.in',
  'xfce-workspaces-settings.desktop.in',
] 
foreach file : desktop_files
  i18n.merge_file(
    input: file,
    output: fs.stem(file),
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )
endforeach
