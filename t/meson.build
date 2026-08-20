
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
  testapp = executable('testapp', test_sources + [slapt_h], link_with: libslapt, dependencies: deps + [check], install: false)
  test('testsuite', testapp, is_parallel: false, workdir: test_path, env: ['ROOT=' + test_path, 'CK_LOG_FILE_NAME=-'])

  malformed_app = executable(
    'testapp_malformed',
    'test_packages_malformed.c',
    link_with: libslapt,
    dependencies: deps + [check],
    override_options: ['b_sanitize=address,undefined', 'b_lto=false'],
    install: false
  )
  test('malformed-input', malformed_app, is_parallel: false)

  fuzz_community_repos = executable(
    'fuzz_community_repos',
    ['fuzz/fuzz_community_repos.c', slapt_h],
    link_with: libslapt,
    dependencies: deps + [check],
    override_options: ['b_sanitize=address,undefined', 'b_lto=false'],
    c_args: ['-fno-sanitize-recover=all'],
    install: false
  )
  test('community-repos-fuzz', fuzz_community_repos, is_parallel: false, timeout: 180)
endif

test('clitest', find_program('clitests.sh'), depends: slapt_get, args: [slapt_get.full_path()])

fuzz_corpus_dir = join_paths(meson.project_source_root(), 't', 'fuzz', 'corpus')
manual_fuzz = executable(
  'manual_fuzz',
  ['fuzz/manual_fuzz.c', slapt_h],
  link_with: libslapt,
  dependencies: deps,
  override_options: ['b_sanitize=address,undefined', 'b_lto=false'],
  c_args: ['-fno-sanitize-recover=all'],
  install: false
)
test('manual-fuzz', manual_fuzz, args: [fuzz_corpus_dir, '42', '50000'], is_parallel: false, timeout: 60)

if get_option('fuzz')
  libfuzzer_runner = executable(
    'libfuzzer_fuzz',
    ['fuzz/fuzz_libfuzzer.c', slapt_h],
    link_with: libslapt,
    dependencies: deps,
    override_options: ['b_lto=false'],
    link_args: ['-fsanitize=fuzzer,address,undefined'],
    install: false
  )
  # put new units in the build dir
  run_command('mkdir', '-p', meson.current_build_dir() / 'fuzz-corpus-run')
  test('libfuzzer-fuzz', libfuzzer_runner, env: ['ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_summary=1:quarantine_size_mb=64'], args: ['-dict=' + test_path + '/fuzz/dictionary', '-max_total_time=240', meson.current_build_dir() / 'fuzz-corpus-run', test_path + '/fuzz/corpus'], timeout: 300)

  # optional corpus merge target
  run_target('libfuzzer-corpus-merge', command: [libfuzzer_runner, '-merge=1', test_path / 'fuzz/corpus',  meson.current_build_dir() / 'fuzz-corpus-run'])
endif
