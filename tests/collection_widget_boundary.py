from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/'src/Collections/NativeCollectionWidgets.h').read_text()
if 'class CollectionWidgetNavigation' in s or 'set_input_handler(' in s or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
 raise SystemExit('LVUI owns collection widget navigation or a peer SDK dependency')
if 'CollectionWidgetNavigationBinding' not in s:raise SystemExit('Collection widget binding missing')
