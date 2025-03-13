project(
  'xfwm4',
  'c',
version : '4.20.0-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.56.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'xfce'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gtk': '>= 3.24.0',
  'libxfce4ui': '>= 4.12.0',
  'libxfce4util': '>= 4.8.0',
  'xfconf': '>= 4.13.0',
  'libwnck': '>= 3.14',
  'libx11': '>= 1.6.7',
  'xinerama': '>= 1.1.4',

  'libxi': '>= 1.7.10',
  'libxres': '>= 1.2.0',
  'libxext': '>= 1.0.0',
  'libepoxy': '>= 1.0',
  'libstartup': '>= 0.5',
  'libxrender': '>= 0.9.10',
  'libxcomposite': '>= 0.2',
  'libxdamage': '>= 1.1.5',
  'libxfixes': '>= 5.0.3',
  'libxrandr': '>= 1.5.2',
  'libxpresent': '>= 1.0',
  'presentproto': '>= 1.1',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4kbd = dependency('libxfce4kbd-private-3', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['libxfce4util'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])
libwnck = dependency('libwnck-3.0', version: dependency_versions['libwnck'])
libx11 = dependency('x11', version: dependency_versions['libx11'])
xinerama = dependency('xinerama', version: dependency_versions['xinerama'])

xdt_csource = find_program('xdt-csource', required: true)

feature_cflags = []

libxi = dependency('xi', version: dependency_versions['libxi'], required: get_option('xi2'))
if libxi.found()
  feature_cflags += '-DHAVE_XI2=1'
endif

libxres = dependency('xres', version: dependency_versions['libxres'], required: get_option('xres'))
if libxres.found()
  feature_cflags += '-DHAVE_XRES=1'
endif

libxext = dependency('xext', version: dependency_versions['libxext'], required: get_option('xsync'))
if libxext.found()
  feature_cflags += '-DHAVE_XSYNC=1'
endif

libepoxy = dependency('epoxy', version: dependency_versions['libepoxy'], required: get_option('epoxy'))
if libepoxy.found()
  feature_cflags += '-DHAVE_EPOXY=1'
endif

libstartup = dependency('libstartup-notification-1.0', version: dependency_versions['libstartup'], required: get_option('startup-notification'))
if libstartup.found()
  feature_cflags += '-DHAVE_LIBSTARTUP_NOTIFICATION=1'
endif

libxrender = dependency('xrender', version: dependency_versions['libxrender'], required: get_option('render'))
if libxrender.found()
  feature_cflags += '-DHAVE_RENDER=1'
endif

libxrandr = dependency('xrandr', version: dependency_versions['libxrandr'], required: get_option('randr'))
if libxrandr.found()
  feature_cflags += '-DHAVE_RANDR=1'
endif

xpresent_deps = []
xpresent_deps += dependency('xpresent', version: dependency_versions['libxpresent'], required: get_option('xpresent'))
xpresent_deps += dependency('presentproto', version: dependency_versions['presentproto'], required: get_option('xpresent'))
enable_xpresent = not get_option('xpresent').disabled()
foreach dep : xpresent_deps
  enable_xpresent = enable_xpresent and dep.found()
endforeach
if enable_xpresent
  feature_cflags += '-DHAVE_PRESENT_EXTENSION=1'
endif

compositor_deps = []
compositor_deps += dependency('xrender', version: dependency_versions['libxrender'], required: get_option('compositor'))
compositor_deps += dependency('xcomposite', version: dependency_versions['libxcomposite'], required: get_option('compositor'))
compositor_deps += dependency('xdamage', version: dependency_versions['libxdamage'], required: get_option('compositor'))
compositor_deps += dependency('xfixes', version: dependency_versions['libxfixes'], required: get_option('compositor'))
enable_compositor = not get_option('compositor').disabled()
foreach dep : compositor_deps
  enable_compositor = enable_compositor and dep.found()
endforeach
if enable_compositor
  feature_cflags += '-DHAVE_COMPOSITOR=1'
endif

if get_option('poswin')
  feature_cflags += '-DSHOW_POSITION=1'
endif

helper_path_prefix = get_option('helper-path-prefix')
if helper_path_prefix == ''
  helper_path_prefix = get_option('prefix') / get_option('libdir')
endif

headers = [
  'math.h',
  'memory.h',
  'stdlib.h',
  'string.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

libm = cc.find_library('m', required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
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
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
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

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
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
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

subdir('common')
subdir('defaults')
subdir('helper-dialog')
subdir('icons')
subdir('po')
subdir('settings-dialogs')
subdir('src')
subdir('themes')
