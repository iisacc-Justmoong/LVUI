from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Color/ColorSurface.h').read_text();host=(root/'src/Color/NativeColorSurface.h').read_text()
if 'class ColorSurface final' in values or 'class NativeColorSurface final' in host or 'set_paint_handler(' in host:
 raise SystemExit('LVUI owns color surface graphics or state')
if 'ColorSurfaceModelPolicy' not in values or 'ColorSurfaceBinding' not in host:raise SystemExit('Color surface binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
 raise SystemExit('LVUI color surface depends directly on another SDK')
