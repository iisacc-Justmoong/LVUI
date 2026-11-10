from pathlib import Path
import re,sys
root=Path(sys.argv[1]);button=(root/'src/Controls/Button.h').read_text();dropdown=(root/'src/Controls/DropdownButton.h').read_text()
button_adapter=(root/'src/Controls/NativeButton.h').read_text();dropdown_adapter=(root/'src/Controls/NativeDropdownButton.h').read_text()
if 'class Button final' in button or 'class DropdownButton final' in dropdown:raise SystemExit('LVUI retains a duplicate visual model')
for name,source,token in (('button',button_adapter,'ButtonBinding'),('dropdown',dropdown_adapter,'DropdownBinding')):
    if token not in source or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
        raise SystemExit(name+' independent binding missing or depends on another SDK')
if any(name in dropdown_adapter for name in ('class NativeDropdownButton final','set_paint_handler(','builder.action_surface(')):
    raise SystemExit('LVUI still owns dropdown rendering or interaction')
