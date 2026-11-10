from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/ListViewport.h").read_text()
if "class ListViewportSource final" in s or "ListViewportBinding" not in s:raise SystemExit("LVUI still owns list viewport geometry")
if "ii::gui::" in s:raise SystemExit("LVUI viewport binding directly depends on iiGUI")
