# Parser lookahead benchmark for PrimJS PR #501

Run on macOS 26.7.1 ARM64 with a Release tracing-GC build. All inputs
come from the generators in this directory. The benchmark uses one engine
build. Its driver sets each new runtime's flags before starting the timer.
The timed region covers `LEPUS_Eval` with compile-only flags or source
compilation plus execution of the top level. Source reads, runtime and
context creation, and result checks are outside the timer. The driver
supplies source and uses no code cache.

The four modes are off, wrapper shortcut only, token kinds only, and all
three optimizations. Each workload and timing mode has eight balanced
rounds, with one warmup and three measured samples per variant per round.
Savings are the median of within-round differences between sample medians.
The summary also reports bootstrap intervals.

## Reproduce on macOS ARM64

Use the PR revision that is the parent of the evidence commit. Install GN,
Ninja, and dependencies using the PrimJS README, including the test262
revision and patches listed in DEPS. Set `PRIMJS` to the absolute path of
that checkout and run the commands below from this evidence directory.

```sh
python3 generate_large_workloads.py workloads
python3 generate_nested_bundle.py workloads/nested-bundle.js
python3 generate_late_slash.py workloads/late-slash.js
(cd workloads && shasum -a 256 -c ../workloads.sha256)

(
  cd "$PRIMJS"
  buildtools/gn/gn gen out/pr501-v3 --args='target_cpu="arm64" mac_deployment_target="15.0" is_debug=false use_lepusng=true enable_quickjs_debugger=true enable_unittests=true'
  buildtools/ninja/ninja -C out/pr501-v3 -d keeprsp qjs_exe run_test262 quickjs_unittest
  out/pr501-v3/quickjs_unittest
  out/pr501-v3/qjs testing/quickjs/compiler/unit_test/parser_lookahead.js
)

python3 build_eval_bench.py "$PRIMJS/buildtools/ninja/ninja" "$PRIMJS/out/pr501-v3" gn
python3 run_large_ab.py "$PRIMJS/out/pr501-v3/eval_bench" workloads macos.csv | tee macos-summary.txt
python3 run_test262_both.py "$PRIMJS" "$PRIMJS/out/pr501-v3"
python3 compare_test262.py test262-0.report test262-1.report
```

The Test262 runner uses a host callback that reads the setting once per
runtime. Both variants reported 1,289 failures across 39,614 cases, with
the same failure paths, 1,314 exclusions, and 5,718 skips. For the RC gtests
and JS tests, use a second build directory and add
`enable_compatible_mm=false enable_primjs_snapshot=false` to the GN args.
