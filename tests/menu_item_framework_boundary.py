from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Overlays/NativeMenuItem.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('LVUI menu item directly depends on another SDK')
for token in ('MenuItemBinding','MenuItemInputValue','MenuItemPortValue','MenuPopupPortValue'):
    if token not in host:raise SystemExit('Independent menu item contract missing: '+token)
for token in ('class NativeMenuItem final','set_paint_handler(','builder.action_surface(','builder.on_retire('):
    if token in host:raise SystemExit('LVUI owns menu item graphics or lifecycle: '+token)
