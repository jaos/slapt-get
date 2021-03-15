
lib_sources = [
  'common.c',
  'configuration.c',
  'package.c',
  'slaptcurl.c',
  'transaction.c',
]
lib_headers = [
  'main.h',
  'common.h',
  'configuration.h',
  'package.h',
  'slaptcurl.h',
  'transaction.h',
]

if libgpgme.found()
  lib_sources += ['slaptgpgme.c']
  lib_headers += ['slaptgpgme.h']
endif

main_sources = [
  'main.c',
  'main.h',
]

custom_target('slapt.h',
  input: lib_headers,
  output: 'slapt.h',
  command: [find_program('mkslapth.sh'), join_paths(meson.project_source_root(), 'src'), '@OUTPUT@', lib_headers],
  install: true,
  install_dir: get_option('includedir')
)

libslapt = library('libslapt', lib_sources, dependencies : deps, install: true, version: version, soversion: 0, name_prefix: '')

if get_option('testbuild').enabled()
  executable('slapt-get', main_sources + lib_sources, dependencies : deps, install: true, install_dir: get_option('sbindir'))
else
  executable('slapt-get', main_sources, link_with: libslapt, dependencies : deps, install: true, install_dir: get_option('sbindir'))
endif

pkgconfig = import('pkgconfig')
pkgconfig.generate(libslapt, name: 'libslapt', description: 'Library for slapt-get, an apt like system for Slackware package management.')
