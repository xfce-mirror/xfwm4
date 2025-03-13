sizes = [16, 48, 128]
names = [
  'org.xfce.workspaces',
  'org.xfce.xfwm4-tweaks',
  'org.xfce.xfwm4',
]

foreach name : names
  foreach size : sizes
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach

  install_data(
    'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  )
endforeach

install_data(
  '48x48' / 'xfwm4-default.png',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '48x48' / 'apps',
)
install_data(
  'scalable' / 'xfwm4-default.svg',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

sizes = [22]
names = [
  'xfce-wm-close',
  'xfce-wm-maximize',
  'xfce-wm-menu',
  'xfce-wm-minimize',
  'xfce-wm-shade',
  'xfce-wm-stick',
  'xfce-wm-unmaximize',
  'xfce-wm-unshade',
  'xfce-wm-unstick',
]

foreach name : names
  foreach size : sizes
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'actions',
    )
  endforeach

  install_data(
    'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'actions',
  )
endforeach
