from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeHierarchyPanel.h").read_text()
if "class NativeHierarchyPanel final" in s or "HierarchyPanelHostPolicy" not in s or "HierarchyPanelBinding" not in s:raise SystemExit("LVUI still owns hierarchy panel host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Hierarchy panel binding directly depends on another SDK")
