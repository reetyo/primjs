"""Generate deterministic standalone JavaScript from scratch."""
from pathlib import Path
import sys

TARGET_BYTES = 2_100_000


def declarations(index):
    return (
        f'function compute{index}(amount, factor, offset) {{'
        f'let total = amount * factor + offset;'
        f'let label = "synthetic record {index:06d}";'
        f'return total + label.length + {index % 71};'
        '}\n'
    )


def properties(index):
    return (
        f'key{index:06d}() {{'
        f'const row = {{label: "synthetic sample {index:06d}", value: {index}}};'
        'return row.label.length + row.value % 73;'
        '},\n'
    )


def generate(prefix, record, suffix):
    parts = [prefix]
    size = len(prefix) + len(suffix)
    index = 0
    while size < TARGET_BYTES:
        row = record(index)
        parts.append(row)
        size += len(row)
        index += 1
    return ("".join(parts) + suffix).encode("ascii")


def mixed_record(index):
    source = properties(index)
    if index % 13 == 0:
        source = source.replace(
            'return row.label.length + row.value % 73;',
            'const rx = /sample/; let v; ({value: v} = row); '
            'return ((a, b) => a + b)(v, rx.test(row.label) ? 1 : 0);',
        )
    return source


out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name, content in {
    "wrapped-functions": generate(
        "(function(){return (()=>{const base = `generated`;\n",
        declarations,
        "return compute0(2, 3, 4) - \"synthetic record 000000\".length + 32;})()})()\n",
    ),
    "fallback-object": generate(
        "const table = { origin: `generated`,\n",
        properties,
        "};\ntable.key000000() - \"synthetic sample 000000\".length + 42;\n",
    ),
    "mixed-expressions": generate(
        "(()=>{const table = { origin: `generated`,\n",
        mixed_record,
        "};\ntable.key000000() + 41;\nreturn table.key000000() + 41;})()\n",
    ),
}.items():
    path = out / (name + ".js")
    path.write_bytes(content)
    print(f"{path.name}\t{len(content)} bytes")
