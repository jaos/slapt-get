
test_sources = [
  'test.c',
  'test_common.c',
  'test_configuration.c',
  'test_curl.c',
  'test_packages.c',
  'test_transaction.c'
]

test_path = join_paths(meson.project_source_root(), 't')

check = dependency('check', required: false)
if check.found()
  testapp = executable('testapp', test_sources, link_with: libslapt, dependencies: deps + [check], install: false)
  test('testsuite', testapp, is_parallel: false, workdir: test_path, env: ['ROOT=' + test_path])
endif

test('clitest', find_program('clitests.sh'), args: [slapt_get.full_path()])
