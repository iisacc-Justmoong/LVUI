from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/TableViewport.h").read_text()
if "class TableViewportSource final" in s or "TableViewportBinding" not in s:raise SystemExit("LVUI still owns table viewport geometry")
if "ii::gui::" in s:raise SystemExit("LVUI table viewport binding directly depends on iiGUI")
