"""Generate the benchmark inputs from scratch with Python 3."""
from pathlib import Path
import sys

output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)


def ordinary(i):
    return (
        f'm{i}:function(n){{const v=({{id:{i},name:"item-{i}",rows:'
        f'[[1,2], [3,4], [5,6]],flags:{{ready:true,valid:false}}}});'
        'const r=([v.rows[0],v.rows[1],v.rows[2]]);'
        'return ((n+v.id)*3)+(r[0][0]+r[1][0]+r[2][0])+v.name.length;},\n'
    )


def fallback(i):
    return (
        f'm{i}:function(n){{const v=({{/* keep ] }} ) */id:{i},'
        'name:`item-${n}`, rows:[[1,2],[3,4],[5,6]]});'
        'const r=([/[)\\]]/.test("]"), (n / 2), v.rows[1]]);'
        'let x; [x] /* destructuring */ = r;'
        'return ((value) => value + v.id)(x ? n : 0);},\n'
    )

def late_fallback(i):
    # The slash forces fallback after a long ASCII prefix in every group.
    additions = "+".join(str(n) for n in range(1, 66))
    return f"m{i}:function(n){{return (n+{additions}+n/3);}},\n"


def build(which):
    return "(function(){const table={\n" + "".join(
        which(i) for i in range(2000)
    ) + "};return typeof table.m1999 === 'function' ? 42 : -1;})()\n"


cases = {
    "ordinary": ordinary,
    "mixed": lambda i: fallback(i) if i % 3 == 0 else ordinary(i),
    "fallback": fallback,
    "late-fallback": late_fallback,
}
for name, recipe in cases.items():
    source = build(recipe)
    fixture = output / f"{name}.js"
    fixture.write_bytes(source.encode("ascii"))
    print(name, fixture.stat().st_size)
