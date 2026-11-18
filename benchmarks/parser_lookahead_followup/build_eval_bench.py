"""Relink the qjs executable with a benchmark main instead of the CLI main."""
from pathlib import Path
import shlex
import subprocess
import sys

ninja, directory, build_kind = sys.argv[1:4]
directory = Path(directory).resolve()
source = Path(__file__).with_name("eval_bench.cc").resolve()
target = "qjs_exe" if build_kind == "gn" else "qjs"
commands = subprocess.check_output(
    [ninja, "-t", "commands", target], cwd=directory, text=True
).splitlines()
compile_cmd, = [shlex.split(line) for line in commands if "/compiler/qjs.cc " in line or line.endswith("/compiler/qjs.cc")]
link_cmd = commands[-1]
obj_index = compile_cmd.index("-o") + 1
original = compile_cmd[obj_index]
compile_cmd[obj_index] = "eval_bench.o"
for i, item in enumerate(compile_cmd):
    if item.endswith("/compiler/qjs.cc"):
        compile_cmd[i] = str(source)
    elif item in (original, original + ".d") and i != obj_index:
        compile_cmd[i] = "eval_bench.o" + (".d" if item.endswith(".d") else "")
subprocess.run(compile_cmd, cwd=directory, check=True)

if build_kind == "gn":
    rsp = (directory / "qjs.rsp").read_text()
    assert rsp.count(original) == 1, original
    (directory / "eval_bench.rsp").write_text(rsp.replace(original, "eval_bench.o"))
    assert "-o ./qjs " in link_cmd
    link_cmd = link_cmd.replace("-o ./qjs ", "-o ./eval_bench ").replace(
        "./qjs.rsp", "./eval_bench.rsp"
    )
else:
    assert original in link_cmd and "-o qjs " in link_cmd
    link_cmd = (
        link_cmd.replace(original, "eval_bench.o")
        .replace("-o qjs ", "-o eval_bench ")
        .replace("-lquick_static", "-llog")
    )
subprocess.run(link_cmd, cwd=directory, shell=True, check=True)
