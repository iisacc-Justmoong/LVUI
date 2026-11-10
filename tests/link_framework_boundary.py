from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/Link.h').read_text();host=(root/'src/Controls/NativeLink.h').read_text()
if 'class Link final' in values or 'class NativeLink final' in host or 'set_paint_handler(' in host:
 raise SystemExit('LVUI owns link graphics or state')
if 'LinkModelPolicy' not in values or 'LinkBinding' not in host:
 raise SystemExit('Independent link binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
 raise SystemExit('LVUI link depends directly on another SDK')
