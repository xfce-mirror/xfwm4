project(
  'xfwm4',
  'c',
  version: run_command(['./version.sh', 'print-version'], capture: true, check: true).stdout().strip(),
  license: 'GPL-2',
  meson_version: '>= 0.58.0',
  default_options: [
    'c_std=gnu99',
    'buildtype=debugoptimized',
    'warning_level=2',
  ]
)
meson.add_dist_script('./version.sh', 'rewrite-version', meson.project_version())

project_namespace = 'xfce'
pkgdatadir = get_option('datadir') / meson.project_name()

cc = meson.get_compiler('c')
gnome = import('gnome')
i18n = import('i18n')

dependency_versions = {
  'glib': '>= 2.66.0',
  'gtk': '>= 3.24.0',
  'libepoxy': '>= 1.0',
  'libx11': '>= 1.6.7',
  'libxfce4kbd_private': '>= 4.12.0',
  'libxfce4ui': '>= 4.12.0',
  'libxfce4util': '>= 4.8.0',
  'presentproto': '>= 1.1',
  'startup_notification': '>= 0.5',
  'wnck': '>= 3.14',
  'xcomposite': '>= 0.2',
  'xdamage': '>= 0',
  'xext': '>= 0',
  'xfconf': '>= 4.18.0',
  'xfixes': '>= 0',
  'xi': '>= 0',
  'xinerama': '>= 0',
  'xpresent': '>= 1.0',
  'xrandr': '>= 0',
  'xrender': '>= 0',
  'xres': '>= 0',
}

# xext is required for xsync support, but if xsync is disabled, we still want
# it for xshape, so we never want to pass 'disabled'
require_xext = get_option('xsync').enabled()

# xrender is needed for the 'render' or 'compositor' options, so we need a new
# feature that's set to auto/enabled/disabled depending on the combined state
# of the two options.
opt_render = get_option('render')
opt_compositor = get_option('compositor')
if opt_render.enabled()
  feature_xrender = opt_render
elif opt_compositor.enabled()
  feature_xrender = opt_compositor
elif opt_render.disabled() and opt_compositor.disabled()
  feature_xrender = opt_render
elif opt_render.auto()
  feature_xrender = opt_render
else
  feature_xrender = opt_compositor
endif

# Required
glib = dependency('glib-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['libxfce4util'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4kbd_private = dependency('libxfce4kbd-private-3', version: dependency_versions['libxfce4kbd_private'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])
wnck = dependency('libwnck-3.0', version: dependency_versions['wnck'])
libx11 = dependency('x11', version: dependency_versions['libx11'])
xinerama = dependency('xinerama', version: dependency_versions['xinerama'])

# Feature: 'startup-notification'
startup_notification = dependency('libstartup-notification-1.0', version: dependency_versions['startup_notification'], required: get_option('startup-notification'))

# Feature: 'epoxy'
libepoxy = dependency('epoxy', version: dependency_versions['libepoxy'], required: get_option('epoxy'))

# Feature: 'xpresent'
xpresent = dependency('xpresent', version: dependency_versions['xpresent'], required: get_option('xpresent'))
presentproto = dependency('presentproto', version: dependency_versions['presentproto'], required: get_option('xpresent'))

# Feature: 'xi2'
xi = dependency('xi', version: dependency_versions['xi'], required: get_option('xi2'))

# Feature: 'randr'
xrandr = dependency('xrandr', version: dependency_versions['xrandr'], required: get_option('randr'))

# Feature: 'compositor'
xcomposite = dependency('xcomposite', version: dependency_versions['xcomposite'], required: opt_compositor)
xfixes = dependency('xfixes', version: dependency_versions['xfixes'], required: opt_compositor)
xdamage = dependency('xdamage', version: dependency_versions['xdamage'], required: opt_compositor)

# Feature: 'xsync', or for XShape
xext = dependency('xext', version: dependency_versions['xext'], required: require_xext)

# Feature: 'render' or Feature: 'compositor'
xrender = dependency('xrender', version: dependency_versions['xrender'], required: feature_xrender)

# Optional; can be used to get the PID of the process owning a window
xres = dependency('xres', version: dependency_versions['xres'], required: false)

# Needed on some systems but not others
libm = cc.find_library('m', required: false)

feature_cflags = [
  '-DWNCK_I_KNOW_THIS_IS_UNSTABLE'
]
if startup_notification.found()
  feature_cflags += '-DHAVE_LIBSTARTUP_NOTIFICATION=1'
endif
if libepoxy.found()
  feature_cflags += '-DHAVE_EPOXY=1'
endif
if xpresent.found() and presentproto.found()
  feature_cflags += '-DHAVE_PRESENT_EXTENSION=1'
endif
if xi.found()
  feature_cflags += '-DHAVE_XI2=1'
endif
if xres.found()
  feature_cflags += '-DHAVE_XRES=1'
endif
if xext.found() and not get_option('xsync').disabled()
  feature_cflags += '-DHAVE_XSYNC=1'
endif
if xrandr.found()
  feature_cflags += '-DHAVE_RANDR=1'
endif
if xrender.found()
  feature_cflags += '-DHAVE_RENDER=1'
  if xcomposite.found() and xfixes.found() and xdamage.found()
    feature_cflags += '-DHAVE_COMPOSITOR=1'
  endif
endif
if get_option('poswin')
  feature_cflags += '-DSHOW_POSITION=1'
endif

headers = [
  'memory.h',
  'stdlib.h',
  'string.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.replace('.', '_').to_upper())
  endif
endforeach

xdt_csource = find_program('xdt-csource', native: true, required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Werror=implicit-function-declaration',
]

buildtype = get_option('buildtype')
if buildtype == 'debug' or buildtype == 'debugoptimized'
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += '-DDEBUG=1'

  if buildtype == 'debug'
    extra_cflags += [
      '-DG_ENABLE_DEBUG',
      '-DDEBUG_TRACE=1',
    ]
  endif
elif buildtype == 'release' or buildtype == 'minsize' or buildtype == 'plain'
  extra_cflags += '-DNDEBUG'

  if buildtype == 'minsize'
    extra_cflags += [
      '-DG_DISABLE_CAST_CHECKS',
      '-DG_DISABLE_ASSERT',
    ]
  endif
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('git')[0].split('.')

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DSHORT_VERSION="@0@.@1@"'.format(version_parts[0], version_parts[1]),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
]

git_rev = run_command('git', 'rev-parse', '--short', 'HEAD', capture: true, check: false).stdout().strip()
if git_rev == ''
  git_rev = 'UNKNOWN'
endif
extra_cflags += '-DREVISION="@0@"'.format(git_rev)

helper_path_prefix = get_option('helper-path-prefix')
if helper_path_prefix == '$libdir'
  helper_path_prefix = get_option('libdir')
  helper_path_prefix_abs = get_option('prefix') / get_option('libdir')
else
  helper_path_prefix_abs = helper_path_prefix
endif
extra_cflags += '-DHELPERDIR="@0@"'.format(helper_path_prefix_abs)

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

subdir('defaults')
subdir('helper-dialog')
subdir('icons')
subdir('common')
subdir('settings-dialogs')
subdir('src')
subdir('themes')
subdir('po')
