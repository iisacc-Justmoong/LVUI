from pathlib import Path
import re,sys
r=Path(sys.argv[1]);model=(r/"src/Collections/HierarchyItem.h").read_text();projection=(r/"src/Collections/HierarchyItemProjection.h").read_text()
if "class HierarchyItem final" in model or "HierarchyItemModelPolicy" not in model or "appearance.key = row.entry.key" in projection:raise SystemExit("Hierarchy item visual model remains in LVUI")
if any(re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s for s in (model,projection)):raise SystemExit("Hierarchy model binding directly imports another SDK")
