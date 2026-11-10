from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/HierarchyToolbar.h").read_text()
if "class HierarchyToolbar final" in s or "HierarchyToolbarModelPolicy" not in s:raise SystemExit("Hierarchy toolbar state remains in LVUI")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Hierarchy toolbar binding imports another SDK")
