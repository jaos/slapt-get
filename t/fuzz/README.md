# t/fuzz - PACKAGES.TXT fuzzing

## adhoc/manual fuzzing

Basic and portable fuzz'ish testing.

```sh
meson setup build
meson compile -C build
meson test -C build
```

## libFuzzer based fuzzing

Requires clang and a static library build.

```sh
CC=clang meson setup build-fuzz -Dfuzz=true \
    -Ddefault_library=static
meson compile -C build-fuzz
meson test -C build-fuzz libfuzzer-fuzz
```

New units land in `build-fuzz/t/fuzz-corpus-run/`.

## merging new units into the corpus

To merge newly found corpus from fuzz runs that are put inside the
build-fuzz/t/fuzz-corpus-run:

```sh
meson compile -C build-fuzz libfuzzer-corpus-merge
```

## debugging a failing fuzz run

Failing inputs land in `build-fuzz/t/fuzz-corpus-run`:

```sh
ls build-fuzz/t/fuzz-corpus-run | grep -E 'crash|timeout'
```

Run the failing input on its own:

```sh
ASAN_OPTIONS=symbolize=1:print_stacktrace=1:halt_on_abort=0 \
UBSAN_OPTIONS=print_stacktrace=1 \
ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer \
build-fuzz/t/libfuzzer_fuzz -runs=0 build-fuzz/t/fuzz-corpus-run/crash-<hash>
```

Shrink the failing input:

```sh
build-fuzz/t/libfuzzer_fuzz -minimize_crash=1 -runs=100000 <failing input>
```

Verify the fix, then merge new units into the corpus:

```sh
build-fuzz/t/libfuzzer_fuzz -runs=0 t/fuzz/corpus
meson test -C build-fuzz
meson test -C build
meson compile -C build-fuzz libfuzzer-corpus-merge
```
