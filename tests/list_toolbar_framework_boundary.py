"""List-toolbar values are independent; iiGUI owns visual behavior."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Collections/NativeListToolbar.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('List toolbar binding directly depends on a graphics SDK')
for required in ('ListToolbarModelPolicy','ListToolbarBinding','NativeIconCache<Decoder>'):
    if required not in host:raise SystemExit('List toolbar lost its independent binding: '+required)
for forbidden in ('class NativeListToolbar final','builder.retain(','builder.on_publish(',
                  'builder.on_retire(','WidgetDrawing','paint_slot('):
    if forbidden in host:raise SystemExit('LVUI still owns list-toolbar behavior: '+forbidden)
if 'class ListToolbar final' in (root/'src/Collections/ListToolbar.h').read_text():
    raise SystemExit('LVUI still owns the list-toolbar geometry model')
panel=(root/'src/Collections/NativeListPanel.h').read_text()
if 'ListPanelBinding' not in panel or 'ToolbarModel' not in panel:
    raise SystemExit('The list panel does not inject the iiGUI list-toolbar model and host')
