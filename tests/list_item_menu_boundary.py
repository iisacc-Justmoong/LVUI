from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/ListItemMenu.h").read_text()
if "class ListItemMenu final" in s or "ListItemMenuBinding" not in s:raise SystemExit("LVUI still owns list item menu validation")
if "ii::gui::" in s:raise SystemExit("LVUI menu binding directly depends on iiGUI")
