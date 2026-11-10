from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Collections/ListItemSegments.h').read_text();host=(root/'src/Collections/NativeListItemSegments.h').read_text()
if 'class ListItemSegments final' in values or 'class NativeListItemSegments final' in host or 'set_paint_handler(' in host:
    raise SystemExit('LVUI owns segment state or graphics')
if 'ListItemSegmentsModelPolicy' not in values or 'ListItemSegmentsBinding' not in host:
    raise SystemExit('Independent list segment binding is missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI list segments depend directly on another SDK')
