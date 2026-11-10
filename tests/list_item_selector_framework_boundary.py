from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Collections/ListItemSelector.h').read_text();host=(root/'src/Collections/NativeListItemSelector.h').read_text()
if 'class ListItemSelector final' in values or 'class NativeListItemSelector final' in host or 'set_paint_handler(' in host:
    raise SystemExit('LVUI owns selector state or graphics')
if 'ListItemSelectorModelPolicy' not in values or 'ListItemSelectorBinding' not in host:
    raise SystemExit('Independent list selector binding is missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI list selector depends directly on another SDK')
