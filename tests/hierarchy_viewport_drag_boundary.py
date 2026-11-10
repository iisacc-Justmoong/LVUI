from pathlib import Path
import sys
b=Path(sys.argv[1])/"src/Collections"
for name in ("HierarchyViewport","HierarchyDrag"):
 s=(b/(name+".h")).read_text()
 if ("class "+name+("Source" if name.endswith("Viewport") else "")+" final") in s or name+"Binding" not in s:raise SystemExit("LVUI still owns "+name+" behavior")
 if "ii::gui::" in s:raise SystemExit("LVUI hierarchy binding directly depends on iiGUI")
