# Reproduce v5 parser lookahead measurements

The measured tree is PrimJS PR #501 commit `df3a9d63e4c61150c64dda2a97d7fd319703b010`.
Build macOS ARM64 Release with tracing GC, using the public dependency
setup in the PrimJS README.

Generate the five inputs from the
[public generators](https://github.com/reetyo/primjs/tree/2dec9433aa450a1cdfd5d435725f6134fa5b10d8/benchmarks/parser_lookahead_v3)
and verify the hashes from that directory. Set `PRIMJS` to the checkout
and run from this directory.

```sh
(
  cd "$PRIMJS"
  buildtools/gn/gn gen out/v5 --args='target_cpu="arm64" mac_deployment_target="15.0" is_debug=false use_lepusng=true enable_quickjs_debugger=true enable_unittests=true'
  buildtools/ninja/ninja -C out/v5 -d keeprsp qjs_exe run_test262 quickjs_unittest
)
python3 build_eval_bench.py "$PRIMJS/buildtools/ninja/ninja" "$PRIMJS/out/v5" gn
python3 run_ab.py "$PRIMJS/out/v5/eval_bench" workloads macos.csv
```

The driver sets the host settings bit for each runtime and checks the
snapshot before starting the timer. Source is read once outside the
timer and no bytecode cache is supplied. Compile measures compile-only
evaluation. Source eval includes compiling and running the top level.
Each workload and timing mode uses eight alternating paired rounds with
one warmup and three measured samples per arm in each round.

To reproduce Test262 with the same build, run:

```sh
python3 run_test262_both.py "$PRIMJS" "$PRIMJS/out/v5"
python3 compare_test262.py test262-0.report test262-1.report
```

On both runs, Test262 reported 1,277 errors in 39,614 cases with the
same failed paths, 1,314 exclusions, and 5,718 skips. The separate
custom allocator probe checks the host-bit snapshot after a transition
from enabled to disabled.
