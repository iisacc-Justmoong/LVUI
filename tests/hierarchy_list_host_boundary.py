from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeHierarchyList.h").read_text()
if "class NativeHierarchyList final" in s or "HierarchyListHostPolicy" not in s or "HierarchyListBinding" not in s:raise SystemExit("LVUI still owns hierarchy list host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Hierarchy list binding directly depends on another SDK")
