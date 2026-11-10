from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/ListDrag.h").read_text()
if "class ListDrag final" in s or "ListDragBinding" not in s:raise SystemExit("LVUI still owns list drag behavior")
if "ii::gui::" in s:raise SystemExit("LVUI drag binding directly depends on iiGUI")
