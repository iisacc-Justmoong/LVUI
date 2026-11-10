from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/Tab.h').read_text();host=(root/'src/Controls/NativeTab.h').read_text()
if 'class Tab final' in values or 'class TabBar final' in values or 'class NativeTab final' in host or 'set_paint_handler(' in host:
 raise SystemExit('LVUI owns tab graphics or state')
if 'TabModelPolicy' not in values or 'TabBarModelPolicy' not in values or 'TabBinding' not in host:raise SystemExit('Independent tab binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
 raise SystemExit('LVUI tab depends directly on another SDK')
