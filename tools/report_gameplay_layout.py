#!/usr/bin/env python3
"""Clang record layout + DWARF member sizes, including private members.

Usage: python3 tools/report_gameplay_layout.py build output-dir [binary]
Requires a Clang compile_commands.json and a binary with debug information.
Raw dumps are kept in output-dir, which should be outside version control.
"""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
binary = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else build / "bin/GuitarPP"
output.mkdir(parents=True, exist_ok=True)
command = next(c for c in json.loads((build / "compile_commands.json").read_text())
               if c["file"].endswith("/src/CGamePlay.cpp"))
argv = shlex.split(command["command"])
argv = argv[:argv.index("-o")] + ["-fsyntax-only", "-Xclang", "-fdump-record-layouts",
                                "-Wpadded", command["file"]]
with (output / "clang-layout.txt").open("w") as out, (output / "padding.txt").open("w") as err:
    subprocess.run(argv, cwd=command["directory"], stdout=out, stderr=err, check=True)
script = output / "layout.gdb"
script.write_text('''set pagination off
python
import gdb,json
result={}
for name in ['CPlayer','CGamePlay','GPPGame','PlayerRenderState']:
 try:
  t=gdb.lookup_type(name)
 except gdb.error:
  continue
 fields=[]
 for f in t.fields():
  if hasattr(f,'bitpos') and f.bitpos is not None:
   fields.append({'name':f.name,'offset':f.bitpos//8,'size':f.type.sizeof,'type':str(f.type)})
 result[name]={'size':t.sizeof,'fields':fields}
print(json.dumps(result,indent=2))
end
''')
data = json.loads(subprocess.check_output(["gdb", "-nx", "-q", "-batch", "-x", str(script), str(binary)], text=True))
dump = (output / "clang-layout.txt").read_text()
lines = ["# Member layout", "", "Offsets and line numbers are relative to an object at a 64-byte boundary.",
         "Natural alignment does not guarantee that address; actual sharing depends on allocation.",
         "Container sizes describe headers, not separately allocated storage.", ""]
for name, record in data.items():
    match = re.search(r"\n\s*0 \| (?:class|struct) " + name +
                      r"\n.*?\n\s*\| \[sizeof=(\d+), dsize=\d+, align=(\d+)", dump, re.S)
    assert match and int(match[1]) == record["size"], name
    record["align"] = int(match[2])
    record["padding"] = record["size"] - sum(f["size"] for f in record["fields"])
    lines += [f"## {name}", "", f"Size {record['size']}; alignment {record['align']}; "
              f"inter-member/tail padding {record['padding']} (nested padding excluded).", "",
              "| Member | Byte interval (inclusive) | Size | Relative cache lines |",
              "|---|---:|---:|---:|"]
    for field in record["fields"]:
        start, size = field["offset"], field["size"]
        end = start + size - 1
        lines.append(f"| `{field['name']}` | {start}–{end} | {size} | {start//64}–{end//64} |")
    lines.append("")
(output / "layout.json").write_text(json.dumps(data, indent=2) + "\n")
(output / "layout.md").write_text("\n".join(lines) + "\n")
