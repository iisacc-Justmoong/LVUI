"""The native event listener is an iiGUI input surface."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
compiler = (root / "src/Compiler/Compiler.cpp").read_text()
cmake = (root / "CMakeLists.txt").read_text()
for name in ("src/Controls/EventListener.h", "src/Runtime/NativeEventListener.h"):
    if (root / name).exists() or name in cmake:
        raise SystemExit("LVUI still owns or installs " + name)
if "ii::gui::NativeEventListener::create(" not in compiler or "<iiGUI/NativeEventListenerView.h>" not in compiler:
    raise SystemExit("EventListener generation does not use iiGUI")
if "lvui::native::NativeEventListener::create(" in compiler or "<LVUI/NativeEventListener.h>" in compiler:
    raise SystemExit("EventListener generation still calls the LVUI host")
