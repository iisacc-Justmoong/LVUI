from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/TableHeader.h").read_text()
if "class TableHeader final" in s or "TableHeaderModelPolicy" not in s or "TableHeaderModelBinding" not in s:raise SystemExit("LVUI still owns table header model")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Table header model binding directly depends on another SDK")
