"""Generate a standalone bundle with nested wrappers and large object literals."""

from pathlib import Path
import sys


TARGET_BYTES = 2_136_000


def fill(target, head, tail, make_entry):
    parts = [head]
    size = len(head) + len(tail)
    index = 0
    while size < target:
        entry = make_entry(index)
        parts.append(entry)
        size += len(entry)
        index += 1
    parts.append(tail)
    return "".join(parts)


def method(index):
    rate = 2 + index % 19
    seed = index % 53
    return (
        f'entry{index:05d}(quantity=3){{const rawTotal=quantity*{rate}+{seed};'
        f'const detail={{label:"synthetic catalog entry {index:05d}",rate:{rate}}};'
        'const adjusted=rawTotal+detail.label.length-detail.rate;'
        'return adjusted>0?adjusted:0;},'
    )


def generated_rule(index):
    return (
        f'function rule{index:05d}(quantity){{const label="synthetic rule {index:05d}";'
        f'const scaled=quantity*{2 + index % 19}+{index % 53};'
        'const matching=/synthetic/.test(label);'
        'const box={name:label,value:scaled};'
        'return matching?box.value:0;}\n'
    )


nested = fill(
    174_000,
    'const region={label:`region-${1}`,matcher:/[a-z]+/,',
    '};\n',
    method,
)
middle = fill(
    615_000,
    'const aggregate={label:`group-${2}`,matcher:/[a-z]+/, nested:()=>{'
    + nested
    + 'return region.entry00000();},',
    '};\n',
    method,
)
tail_wrapper = fill(
    349_000,
    'const adjustment=(()=>{const marker=`generated-${3}`;\n',
    'return rule00000(11);})();\n',
    generated_rule,
)
opening = '(function(){return (()=>{const marker=`bundle-${4}`;\n'
closing = tail_wrapper + 'return catalog.base()+adjustment;})()})()\n'
catalog = fill(
    TARGET_BYTES - len(opening) - len(closing),
    'const catalog={base(){return 20;},label:`catalog-${5}`,matcher:/\\d+/, aggregate:()=>{'
    + middle
    + 'return aggregate.entry00000();},',
    '};\n',
    method,
)
output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
source = (opening + catalog + closing).encode("ascii")
output.write_bytes(source)
print(f"{output.name}\t{len(source)} bytes")
