"""Generate a nested object with a division operator near the end."""
from pathlib import Path
import sys

fields = "".join(f"k{i}:{i % 101}," for i in range(12_000))
source = "const obj={" + "child:{" * 32 + fields + "end:12/3" + "}" * 33 + "; 42;\n"
Path(sys.argv[1]).write_text(source, encoding="ascii")
