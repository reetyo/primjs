from pathlib import Path
import os
import shlex
import subprocess
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: run_test262_both.py PRIMJS_CHECKOUT BUILD_DIR")
work = Path(__file__).resolve().parent
repo = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
ninja = str(repo / "buildtools/ninja/ninja")
commands = subprocess.check_output(
    [ninja, "-t", "commands", "run_test262"], cwd=out, text=True
).splitlines()
compile_args = next(
    shlex.split(line) for line in commands if "-c ../../testing/quickjs/run-test262.cc " in line
)
source = compile_args.index("../../testing/quickjs/run-test262.cc")
file = work / "test_host_settings.cc"
compile_args[source] = str(file)
compile_args[compile_args.index("-o") + 1] = "test_host_settings.o"
for flag in ("-MF",):
    pos = compile_args.index(flag)
    del compile_args[pos:pos + 2]
compile_args.remove("-MD")
subprocess.run(compile_args, cwd=out, check=True)
link = shlex.split(commands[-1])
link[link.index("-o") + 1] = "./run_test262_host"
subprocess.run(link + ["test_host_settings.o"], cwd=out, check=True)
processes = []
for mode in ("0", "1"):
    log = (work / f"test262-{mode}.log").open("w")
    command = [str(out / "run_test262_host"), "-m", "-a", "-c",
               str(out / "quickjs_test/test262.conf"), "-r", str(work / f"test262-{mode}.report")]
    process = subprocess.Popen(command, cwd=repo, env=dict(os.environ, PRIMJS_REVIEW_LOOKAHEAD=mode),
                               stdout=log, stderr=subprocess.STDOUT)
    processes.append((mode, process, log))
for mode, process, log in processes:
    result = process.wait()
    log.close()
    print(f"Test262 mode {mode} exit {result}", flush=True)
