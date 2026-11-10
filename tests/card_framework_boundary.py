from pathlib import Path
import re,sys
r=Path(sys.argv[1]);values=(r/'src/Controls/Card.h').read_text();host=(r/'src/Controls/NativeCard.h').read_text()
if 'class Card final' in values or 'class NativeCard final' in host or 'set_paint_handler(' in host:raise SystemExit('LVUI owns card graphics or state')
if 'CardModelPolicy' not in values or 'CardBinding' not in host:raise SystemExit('Independent card binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):raise SystemExit('LVUI card depends directly on another SDK')
