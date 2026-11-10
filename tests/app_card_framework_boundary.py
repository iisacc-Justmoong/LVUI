"""AppCard widget behavior belongs to iiGUI, not the language package."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
shape = (root / "src/Controls/AppCard.h").read_text()
compiler = (root / "src/Compiler/Compiler.cpp").read_text()
cmake = (root / "CMakeLists.txt").read_text()
if "enum class AppCardShape" not in shape or "class AppCard final" in shape:
    raise SystemExit("LVUI must keep the declarative shape while iiGUI owns the model")
if (root / "src/Controls/NativeAppCard.h").exists() or "src/Controls/NativeAppCard.h" in cmake:
    raise SystemExit("LVUI still owns or installs the AppCard native widget")
if "ii::gui::NativeAppCard::create(" not in compiler or "<iiGUI/NativeAppCardView.h>" not in compiler:
    raise SystemExit("AppCard generation does not use the iiGUI widget")
if "lvui::native::NativeAppCard::create(" in compiler or "<LVUI/NativeAppCard.h>" in compiler:
    raise SystemExit("AppCard generation still calls the LVUI widget")
