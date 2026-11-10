from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeTableCellItem.h").read_text()
if "class NativeTableCellItem final" in s or "TableCellItemHostPolicy" not in s or "TableCellItemBinding" not in s:
 raise SystemExit("LVUI table cell still owns its visual host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:
 raise SystemExit("LVUI table cell directly depends on another SDK")
