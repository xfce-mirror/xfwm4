settings_ui_files = [
  {
    'inp': 'xfwm4-workspace-dialog.glade',
    'out': 'xfwm4-workspace-dialog_ui.h',
    'sym': 'workspace_dialog_ui',
  },
  {
    'inp': 'xfwm4-dialog.glade',
    'out': 'xfwm4-dialog_ui.h',
    'sym': 'xfwm4_dialog_ui',
  },
  {
    'inp': 'xfwm4-tweaks-dialog.glade',
    'out': 'xfwm4-tweaks-dialog_ui.h',
    'sym': 'tweaks_dialog_ui',
  },
]

settings_gen_sources = {}

foreach f : settings_ui_files
  settings_gen_sources += {
    f['sym']: custom_target(
      f['sym'],
      output: f['out'],
      input: f['inp'],
      command: [
        xdt_csource,
        '--static',
        '--strip-comments',
        '--strip-content',
        '--name=@0@'.format(f['sym']),
        '--output=@OUTPUT@',
        '@INPUT@'
      ],
    )
  }
endforeach

workspace_res_source = gnome.compile_resources(
  'workspace_resource',
  'workspace.gresource.xml',
)

executable(
  'xfwm4-settings',
  [
    'xfwm4-settings.c',
    'range-debouncer.c',
    settings_gen_sources['xfwm4_dialog_ui'],
  ],
  c_args: [
    '-DG_LOG_DOMAIN="xfwm4-settings"',
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    libxfce4kbd_private,
    xfconf,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
)

executable(
  'xfwm4-tweaks-settings',
  [
    'tweaks-settings.c',
    'range-debouncer.c',
    settings_gen_sources['tweaks_dialog_ui'],
  ],
  c_args: [
    '-DG_LOG_DOMAIN="xfwm4-tweaks-settings"',
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4ui,
    xfconf,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
)

executable(
  'xfwm4-workspace-settings',
  [
    'workspace-settings.c',
    workspace_res_source,
    settings_gen_sources['workspace_dialog_ui'],
  ],
  c_args: [
    '-DG_LOG_DOMAIN="xfwm4-workspace-settings"',
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    libxfce4ui,
    xfconf,
    wnck,
  ],
  link_with: [
    libxfwm_common,
  ],
  install: true,
)

settings_desktop_in_files = [
  'xfce-wm-settings.desktop.in',
  'xfce-wmtweaks-settings.desktop.in',
  'xfce-workspaces-settings.desktop.in',
]

foreach f: settings_desktop_in_files
  i18n.merge_file(
    output: f.replace('.in', ''),
    input: f,
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )
endforeach
