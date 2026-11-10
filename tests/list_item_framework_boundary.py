"""The list item host and its drawing must be owned by iiGUI."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
host = (root / "src/Collections/NativeListItem.h").read_text()
drawing = (root / "src/Collections/NativeListItemDrawing.h").read_text()
if "using ListItemBinding=Host<ListItemHostPolicy<" not in host:
    raise SystemExit("LVUI list item does not use the iiGUI host")
if "using ListItemDrawingBinding=Drawing<ListItemDrawingPolicy<" not in drawing:
    raise SystemExit("LVUI list item does not use iiGUI drawing")
for forbidden in ("class NativeListItem final", "builder.action_surface(", "set_paint_handler("):
    if forbidden in host:
        raise SystemExit("LVUI still owns list item host behavior: " + forbidden)
for forbidden in ("texts.push_back(", "panels.push_back(", "images.push_back("):
    if forbidden in drawing:
        raise SystemExit("LVUI still owns list item drawing: " + forbidden)

contract=(root/"src/Collections/ListItem.h").read_text()
if "class ListItem final" in contract:
    raise SystemExit("LVUI still owns the list-item state and geometry model")
if "using ListItemModelBinding=Model<ListItemModelPolicy>" not in drawing:
    raise SystemExit("The application bridge does not expose the iiGUI list-item model")
composite=(root/"src/Collections/NativeListItemComposite.h").read_text()
if "using ListItemModel=typename Components::ListItemModel" not in composite or "ListItemCompositeBinding" not in composite:
    raise SystemExit("The composite row does not accept an injected list-item model")
if "ii::gui::" in host or "ii::gui::" in drawing or "ii::gui::" in composite:
    raise SystemExit("LVUI directly depends on iiGUI")
compiler=(root/"src/Compiler/Compiler.cpp").read_text()
for declaration in ("ii::gui::BasicNativeListItem", "ii::gui::BasicListItemDrawing", "ii::gui::BasicListItem"):
    if declaration not in compiler:
        raise SystemExit("Generated applications do not compose the iiGUI implementation: " + declaration)
