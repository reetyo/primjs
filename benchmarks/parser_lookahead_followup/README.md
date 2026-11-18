# Reproduce the parser lookahead follow-up

The baseline is the original lightweight parser lookahead from PrimJS PR
#501, at `4a34f602e82df8ba9c03521c7693593a0e366bcb`. The follow-up adds a
prefix check for function and empty-parameter arrow expressions and can skip
materializing simple identifier and string values during token lookahead.
The default PrimJS setting leaves both follow-up optimizations disabled.
A host can enable both with the shared `primjs_fast_parser_lookahead` setting.

Generate the public inputs independently from scratch. The three workloads
are roughly 2.1 MB each and exercise nested wrappers, object expressions
that fall back to token lookahead, and a mixture of those patterns.

```sh
python3 generate_large_workloads.py workloads
shasum -a 256 workloads/*.js
```

The experimental trial modes are 0 (off), 1 (wrapper prefixes), 2
(token kinds), and 3 (both). The production code uses the single host flag.
The separate modes exist only in `bench-override.patch`, which reads the
trial mode once per parser initialization. Test262 results may also be
reproduced with the equivalent `test262-override.patch`; in the pinned
source revision its two settings bits have no other consumers.
Apply only one of the two overrides at a time.

```sh
git clone https://github.com/reetyo/primjs.git primjs
git -C primjs checkout --detach 4a34f602e82df8ba9c03521c7693593a0e366bcb
EVIDENCE="$PWD"
PRIMJS="$PWD/primjs"
git -C "$PRIMJS" apply "$EVIDENCE/followup.patch"
git -C "$PRIMJS" apply "$EVIDENCE/bench-override.patch"
```

Install the public dependencies according to the PrimJS README. Test262
is pinned to `997888324e42013578eb935c2a18a2027ecb8c81` in `DEPS`. Apply
the repository's Test262 patch if installing it manually.

## macOS ARM64

```sh
cd "$PRIMJS"
gn gen out/Release --args='target_cpu="arm64" mac_deployment_target="15.0" is_debug=false use_lepusng=true enable_quickjs_debugger=true enable_unittests=true'
ninja -C out/Release -d keeprsp qjs_exe run_test262
python3 "$EVIDENCE/build_eval_bench.py" "$(command -v ninja)" out/Release gn
python3 "$EVIDENCE/run_large_ab.py" "$PRIMJS/out/Release/eval_bench" "$EVIDENCE/workloads" "$EVIDENCE/macos.csv"
for mode in 0 1 2 3; do
  PRIMJS_BENCH_LOOKAHEAD="$mode" out/Release/qjs testing/quickjs/compiler/unit_test/parser_lookahead.js
done
for mode in 0 3; do
  PRIMJS_BENCH_LOOKAHEAD="$mode" out/Release/run_test262 -m -a -c out/Release/quickjs_test/test262.conf -r "$EVIDENCE/macos-$mode.report"
done
python3 "$EVIDENCE/compare_test262.py" "$EVIDENCE/macos-0.report" "$EVIDENCE/macos-3.report"
```

For the reference-counting edge cases, add
`enable_compatible_mm=false enable_primjs_snapshot=false` to the GN
arguments, build `qjs_exe` in a separate directory and run the same cases.

## Android ARM64

Use Android NDK r27c, CMake, Ninja and a booted Android ARM64 device or
emulator. These commands use a separate staging directory on the device.

```sh
cd "$PRIMJS"
cmake -S . -B out/AndroidRelease -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-UQJS_UNITTEST -DCMAKE_CXX_FLAGS=-UQJS_UNITTEST \
  -DENABLE_UNITTESTS=ON -DENABLE_LEPUSNG=ON -DENABLE_COMPATIBLE_MM=ON \
  -DENABLE_PRIMJS_SNAPSHOT=ON -DENABLE_QUICKJS_DEBUGGER=ON -DENABLE_CODECACHE=ON
ninja -C out/AndroidRelease qjs
```

The pinned CMake CLI links against a legacy `-lquick_static` entry. If that
link reports a missing library, retain the compiled objects. The benchmark
helper links the timing program with the existing static library and
substitutes Android's `-llog` for that legacy link entry.

```sh
python3 "$EVIDENCE/build_eval_bench.py" "$(command -v ninja)" out/AndroidRelease cmake
REMOTE=/data/local/tmp/primjs-public-lookahead
adb -s "$SERIAL" shell "mkdir -p '$REMOTE'"
adb -s "$SERIAL" push out/AndroidRelease/eval_bench "$EVIDENCE"/workloads/*.js "$REMOTE/"
adb -s "$SERIAL" shell "chmod 755 '$REMOTE/eval_bench'"
python3 "$EVIDENCE/run_large_ab.py" "$REMOTE/eval_bench" "$EVIDENCE/workloads" "$EVIDENCE/android.csv" --adb "$(command -v adb)" "$SERIAL"
```

For Android Test262 and edge checks, configure a separate Android build with
the same options except the two `-UQJS_UNITTEST` flags. Build `qjs` and
`run-test262`. Substitute `-llog` for `-lquick_static` in the final link
commands if necessary. Push both programs, `parser_lookahead.js`,
`test262.conf`, `test262_errors.txt`, and the Test262 `test` and `harness`
directories to `$REMOTE`. Put the latter two under `$REMOTE/test262`.
From `$REMOTE`, run the same edge checks and Test262 commands shown for
macOS, using the local executable and configuration paths. Pull the two
reports for comparison.

## Timing

Each scenario uses eight rounds in balanced four-way order. Each process
runs one warm-up and three timed samples with fresh tracing-GC runtimes and
contexts. Inputs are read outside the timer. Compilation uses
`LEPUS_EVAL_FLAG_COMPILE_ONLY`. Source evaluation also runs the top level
and verifies the result. No code cache is supplied. Reported savings are
the median of differences between paired round medians. The intervals
are 95% percentile bootstrap intervals across rounds.
