from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeHierarchyItem.h").read_text()
if "class NativeHierarchyItem final" in s or "HierarchyItemHostPolicy" not in s or "HierarchyItemBinding" not in s:raise SystemExit("LVUI still owns hierarchy item host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI hierarchy item directly depends on another SDK")
