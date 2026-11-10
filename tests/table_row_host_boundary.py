from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeTableRow.h").read_text()
if "class NativeTableRow final" in s or "TableRowHostPolicy" not in s or "TableRowBinding" not in s:raise SystemExit("LVUI still owns table row widget behavior")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI table row directly depends on another SDK")
