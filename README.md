# Reproduce the PrimJS parser lookahead A/B

The baseline is public PrimJS commit `8f09025c0ff0e219a5f06973af43be8dc9f06c78`.
Fetch the proposed change from the PR branch shown below. Use sibling
checkouts and set `BASE` and `PATCH` to their absolute paths.

```sh
git clone https://github.com/lynx-family/primjs.git primjs-base
git -C primjs-base checkout --detach 8f09025c0ff0e219a5f06973af43be8dc9f06c78
git clone --single-branch --branch codex/primjs-fast-lookahead https://github.com/reetyo/primjs.git primjs-patch
git -C primjs-patch checkout --detach 4a406c9cd66ac59a55a1013c0a36dc062c6889b8
BASE="$PWD/primjs-base"
PATCH="$PWD/primjs-patch"
```

Install public dependencies in both checkouts as described in the PrimJS
README. Test262 is pinned to `997888324e42013578eb935c2a18a2027ecb8c81` in
`DEPS`. Apply the repository's Test262 patch if fetching manually.

Generate the synthetic JavaScript. The benchmark uses fresh runtimes and
contexts. It reads the JS once outside the timed region. `compile` measures
`LEPUS_Eval` with `LEPUS_EVAL_FLAG_COMPILE_ONLY`. `eval` compiles and
executes the top level. It also checks the result after the timer stops.
The harness checks that both builds use tracing GC and does not provide
cached bytecode.

```sh
python3 generate_workloads.py workloads
```

## macOS ARM64

In each checkout, build the CLI using the same settings. The deployment
target below accommodates the libc++ headers in the installed macOS 27 SDK.

```sh
gn gen out/Release --args='target_cpu="arm64" mac_deployment_target="15.0" is_debug=false use_lepusng=true enable_quickjs_debugger=true'
ninja -C out/Release -d keeprsp qjs_exe
```

Use clean build directories or relink the CLI with `-d keeprsp` so GN's
`qjs.rsp` remains available. From the evidence directory, point at the two
build directories. The scripts reuse the qjs compile and link settings
with a small benchmark entry point.

```sh
python3 build_eval_bench.py "$(command -v ninja)" "$BASE/out/Release" gn
python3 build_eval_bench.py "$(command -v ninja)" "$PATCH/out/Release" gn
python3 run_ab.py "$BASE/out/Release/eval_bench" "$PATCH/out/Release/eval_bench" workloads macos.csv
```

For Test262, build each variant with the same GN arguments plus
`enable_unittests=true` into `out/Default_snapshot` and run
`ninja -C out/Default_snapshot run_test262 quickjs_unittest qjs_debug_test qjs_exe`.
Run the following for each variant with a distinct report filename.

```sh
out/Default_snapshot/run_test262 -m -a -c out/Default_snapshot/quickjs_test/test262.conf -r "$REPORT"
out/Default_snapshot/quickjs_unittest
out/Default_snapshot/qjs_debug_test
python3 tools/ci/run_quickjs_unittests.py -b out/Default_snapshot
```

Run `python3 compare_test262.py "$BASE_REPORT" "$PATCH_REPORT"` after both
runs. For the reference-counted parser, add `enable_compatible_mm=false
enable_primjs_snapshot=false` to the GN arguments, rebuild `qjs_exe`, and
run `testing/quickjs/compiler/unit_test/parser_lookahead.js` with `qjs`.

## Android ARM64

Build with Android NDK r27c and CMake/Ninja. In each checkout:

```sh
cmake -S . -B out/AndroidRelease -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS=-UQJS_UNITTEST -DCMAKE_CXX_FLAGS=-UQJS_UNITTEST \
  -DENABLE_UNITTESTS=ON -DENABLE_LEPUSNG=ON -DENABLE_COMPATIBLE_MM=ON \
  -DENABLE_PRIMJS_SNAPSHOT=ON -DENABLE_QUICKJS_DEBUGGER=ON -DENABLE_CODECACHE=ON
ninja -C out/AndroidRelease qjs
```

At the pinned upstream revision the CMake CLI link refers to legacy
`-lquick_static`, which does not exist. If the link fails at that point,
leave the compiled objects in place. The benchmark script below reuses
the link command with the existing `libquickjs.a` and Android's `-llog`.
For a standalone `qjs` or `run-test262`, apply the same substitution to
the final link command shown by `ninja -t commands <target>`.

```sh
python3 build_eval_bench.py "$(command -v ninja)" "$BASE/out/AndroidRelease" cmake
python3 build_eval_bench.py "$(command -v ninja)" "$PATCH/out/AndroidRelease" cmake
adb -s "$SERIAL" shell 'mkdir -p /data/local/tmp/primjs-lookahead'
adb -s "$SERIAL" push "$BASE/out/AndroidRelease/eval_bench" /data/local/tmp/primjs-lookahead/base
adb -s "$SERIAL" push "$PATCH/out/AndroidRelease/eval_bench" /data/local/tmp/primjs-lookahead/patch
adb -s "$SERIAL" push workloads/*.js /data/local/tmp/primjs-lookahead/
adb -s "$SERIAL" shell 'chmod 755 /data/local/tmp/primjs-lookahead/base /data/local/tmp/primjs-lookahead/patch'
python3 run_ab.py /data/local/tmp/primjs-lookahead/base /data/local/tmp/primjs-lookahead/patch workloads android.csv adb -s "$SERIAL"
```

The Android CMake target needs `ENABLE_UNITTESTS=ON` to emit the CLI
targets. The `-UQJS_UNITTEST` flags remove the test-mode lexer check from
the timed builds. For Android Test262, compile the runner without those
two `-U` flags and place the pinned `test262/test` and `test262/harness`
directories next to the PrimJS `test262.conf` and `test262_errors.txt`.
Copy them and the two runners to the device. Run both as on macOS, with
separate report paths. For the edge cases, run the patched `qjs` against
`testing/quickjs/compiler/unit_test/parser_lookahead.js`.

Each workload and timing mode has ten paired rounds. Each process runs
two warmups and five recorded samples. Order alternates by round. The
reported baseline and patch times are medians of the five-sample block
medians. The reported saving is the median of the within-round
differences. Positive savings indicate faster patched runs.
