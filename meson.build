project('slapt-get', 'c',
  version: '0.11.8',
  license: 'GPLv2',
  meson_version: '>= 0.56',
  default_options: [
    'buildtype=debugoptimized',
    'b_lto=true',
    'sysconfdir=/etc',
    'warning_level=3',
    'werror=true',
  ],
)
package = meson.project_name()
version = meson.project_version()
i18n = import('i18n')

# paths
prefix = get_option('prefix')
localedir = join_paths(prefix, get_option('localedir'))
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, package)
docdatadir = join_paths(datadir, 'doc', package + '-' + version)
sbindir = join_paths(get_option('prefix'), get_option('sbindir'))
rcdir = join_paths(get_option('sysconfdir'), 'slapt-get')
rcpath = join_paths(rcdir, 'slapt-getrc')

add_project_arguments('-DPACKAGE="' + package + '"', language: 'c')
add_project_arguments('-DGETTEXT_PACKAGE="' + package + '"', language: 'c')
add_project_arguments('-DVERSION="' + version + '"', language: 'c')
add_project_arguments('-DRC_LOCATION="' + rcpath + '"', language: 'c')
add_project_arguments('-DRC_DIR="' + rcdir + '"', language: 'c')
add_project_arguments('-DENABLE_NLS', language: 'c')
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + localedir + '"', language: 'c')

cc = meson.get_compiler('c')
# compiler based checks
deps = [
  dependency('libcurl'),
  dependency('zlib'),
  dependency('openssl'),
  cc.find_library('m')
]
libgpgme = dependency('gpgme', required: false)
if libgpgme.found()
  add_project_arguments('-DSLAPT_HAS_GPGME', language: 'c')
  deps += libgpgme
endif

cflags = [
  '-ffat-lto-objects',
  '-fno-strict-aliasing',
  '-Warith-conversion',
  '-Warray-bounds=2',
  '-Wcast-align=strict',
  '-Wduplicated-cond',
  '-Wduplicated-branches',
  '-Wformat-nonliteral',
  '-Wformat-overflow=2',
  '-Wformat-security',
  '-Wformat=2',
  '-Wimplicit-function-declaration',
  '-Wimplicit-fallthrough=3',
  '-Wincompatible-pointer-types',
  '-Winit-self',
  '-Wint-conversion',
  '-Wlogical-op',
  '-Wmaybe-uninitialized',
  '-Wmissing-field-initializers',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Wparentheses',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wshift-overflow=2',
  '-Wstrict-overflow',
  '-Wstrict-prototypes',
  '-Wstringop-overflow=4',
  '-Wswitch-default',
  '-Wtrampolines',
  '-Wundef',
]
if get_option('testbuild').enabled()
  cflags += [
    '-D_FORTIFY_SOURCE=2',
    '-fno-omit-frame-pointer',
    '-fstack-protector-strong',
    '-ggdb',
  ]
endif
add_project_arguments(cc.get_supported_arguments(cflags), language: 'c')
add_project_link_arguments(cc.get_supported_link_arguments(cflags), language: 'c')
lflags = ['-Wl,-z,defs',
  '-Wl,-z,noexecstack',
  '-Wl,-z,now',
  '-Wl,-z,relro',
  '-Wl,-z,separate-code',
]
add_project_link_arguments(cc.get_supported_link_arguments(lflags), language: 'c')

subdir('po')
subdir('src')
subdir('doc')
subdir('t')

datadocs = [
  'COPYING',
  'ChangeLog',
  'FAQ',
  'README',
  'default.slapt-getrc.aarch64',
  'default.slapt-getrc.arm',
  'default.slapt-getrc.i386',
  'default.slapt-getrc.x86_64',
  'example.slapt-getrc',
]
if build_machine.cpu() == 'x86_64'
  install_data('default.slapt-getrc.x86_64', install_dir: rcdir, rename: 'slapt-getrc.new')
elif build_machine.cpu() == 'aarch64'
  install_data('default.slapt-getrc.aarch64', install_dir: rcdir, rename: 'slapt-getrc.new')
elif build_machine.cpu().startswith('arm')
  install_data('default.slapt-getrc.arm', install_dir: rcdir, rename: 'slapt-getrc.new')
elif build_machine.cpu().startswith('i')
  install_data('default.slapt-getrc.i386', install_dir: rcdir, rename: 'slapt-getrc.new')
endif
install_data(datadocs, install_dir: docdatadir)
