from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeTable.h").read_text()
if "class NativeTable final" in s or "TableBinding" not in s:raise SystemExit("LVUI still owns table rendering and input")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Table binding directly depends on another SDK")
