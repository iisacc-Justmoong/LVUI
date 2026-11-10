from pathlib import Path
import sys
s=(Path(sys.argv[1])/"src/Collections/NativeVirtualCollection.h").read_text()
if "class VirtualViewport final" in s or "VirtualViewportBinding" not in s:raise SystemExit("LVUI still owns virtual viewport state")
if "ii::gui::" in s:raise SystemExit("LVUI virtual viewport binding directly depends on iiGUI")
