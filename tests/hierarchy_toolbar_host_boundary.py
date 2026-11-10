from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeHierarchyToolbar.h").read_text()
if "class NativeHierarchyToolbar final" in s or "HierarchyToolbarHostPolicy" not in s or "HierarchyToolbarBinding" not in s:raise SystemExit("LVUI still owns hierarchy toolbar host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI hierarchy toolbar directly depends on another SDK")
