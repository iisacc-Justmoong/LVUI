from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Color/ColorPickerButton.h').read_text();host=(root/'src/Color/NativeColorPickerButton.h').read_text()
if 'class ColorPickerButton final' in values or 'class NativeColorPickerButton final' in host or 'set_paint_handler(' in host:
    raise SystemExit('LVUI still owns color button graphics or state')
if 'ColorPickerButtonModelPolicy' not in values or 'ColorPickerButtonBinding' not in host:
    raise SystemExit('Independent color button binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI color button depends directly on another SDK')
