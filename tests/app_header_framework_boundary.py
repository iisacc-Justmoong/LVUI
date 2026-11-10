"""AppHeader is generated against iiGUI and owns no widget in LVUI."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
compiler = (root / "src/Compiler/Compiler.cpp").read_text()
cmake = (root / "CMakeLists.txt").read_text()
for name in ("AppHeader.h", "NativeAppHeader.h"):
    if (root / "src/Controls" / name).exists() or f"src/Controls/{name}" in cmake:
        raise SystemExit("LVUI still owns or installs " + name)
if "ii::gui::NativeAppHeader::create(" not in compiler or "<iiGUI/NativeAppHeaderView.h>" not in compiler:
    raise SystemExit("AppHeader generation does not use the iiGUI widget")
if "lvui::native::NativeAppHeader::create(" in compiler or "<LVUI/NativeAppHeader.h>" in compiler:
    raise SystemExit("AppHeader generation still calls the LVUI widget")
