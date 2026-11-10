from pathlib import Path
import re,sys
root=Path(sys.argv[1]); source=(root/'src/Overlays/MenuItem.h').read_text()
if 'class MenuItem final' in source: raise SystemExit('LVUI owns menu item visual calculations')
if 'MenuItemModelPolicy' not in source: raise SystemExit('Independent menu item design policy missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI menu item design depends directly on another SDK')
