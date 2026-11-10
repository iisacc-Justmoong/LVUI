from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/NativeCollectionNavigation.h").read_text()
if "class CollectionNavigation final" in s or "CollectionNavigationBinding" not in s:raise SystemExit("LVUI still owns collection navigation behavior")
if "ii::gui::" in s:raise SystemExit("LVUI navigation binding directly depends on iiGUI")
