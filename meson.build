project('slapt-get', 'c',
  version: '0.11.5',
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
add_project_arguments('-DENABLE_NLS', language: 'c')
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + localedir + '"', language: 'c')

# compiler based checks
libcurl = dependency('libcurl')
zlib = dependency('zlib')
openssl = dependency('openssl')
libgpgme = dependency('gpgme', required: false)
if libgpgme.found()
  add_project_arguments('-DSLAPT_HAS_GPGME', language: 'c')
endif
cc = meson.get_compiler('c')
libm = cc.find_library('m')

deps = [libcurl, zlib, openssl, libm, libgpgme]

cflags = ['-ffat-lto-objects', '-Wshadow', '-Wstrict-overflow', '-fno-strict-aliasing', '-Werror=implicit-function-declaration', '-Werror=implicit-fallthrough=3', '-Werror=maybe-uninitialized', '-Werror=missing-field-initializers', '-Werror=incompatible-pointer-types', '-Werror=int-conversion', '-Werror=redundant-decls', '-Werror=parentheses', '-Wformat-nonliteral', '-Wformat-security', '-Wformat', '-Winit-self', '-Wmaybe-uninitialized', '-Wold-style-definition', '-Wredundant-decls', '-Wstrict-prototypes']
if get_option('testbuild').enabled()
  cflags += ['-fstack-protector', '-ggdb', '-fno-omit-frame-pointer']
endif
add_project_arguments(cc.get_supported_arguments(cflags), language: 'c')
add_project_link_arguments(cc.get_supported_link_arguments(cflags), language: 'c')

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
