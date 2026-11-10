from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeTableHeader.h").read_text()
if "class NativeTableHeader final" in s or "TableHeaderHostPolicy" not in s or "TableHeaderBinding" not in s:raise SystemExit("LVUI still owns table header host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Table header binding directly depends on another SDK")
