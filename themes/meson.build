dirs = [
  'daloa',
  'default',
  'default-hdpi',
  'default-xhdpi',
  'kokodi',
  'moheli',
]
svgs = []

foreach dir : dirs
  pngs = run_command(
    'sh', '-c', 'echo @0@'.format(dir) / '*.png',
    check: true,
  ).stdout().strip().split(' ')
  xpms = run_command(
    'sh', '-c', 'echo @0@'.format(dir) / '*.xpm',
    check: true,
  ).stdout().strip().split(' ')
  install_data(
    pngs,
    xpms,
    dir / 'README',
    dir / 'themerc',
    install_dir: get_option('prefix') / get_option('datadir') / 'themes' / dir[0].to_upper() + dir.substring(1) / 'xfwm4',
  )

  output = run_command(
    'sh', '-c', 'echo @0@'.format(meson.project_source_root() / 'themes' / dir) / '*.svg',
    check: true,
  ).stdout().strip()
  if not output.endswith('*.svg')
    svgs += output.split(' ')
  endif
endforeach

run_target(
  'vacuum-defs',
  command: ['inkscape', '--without-gui', '--vacuum-defs'] + svgs,
)

run_target(
  'update-png',
  command: ['sh', '-c', 'for svg in ' + ' '.join(svgs) + '; do rsvg-convert --format png --output "${svg%.svg}.png" "$svg"; done'],
)
