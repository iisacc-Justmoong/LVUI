from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/RadioButton.h').read_text();host=(root/'src/Controls/NativeRadioButton.h').read_text()
if 'class RadioButton final' in values:raise SystemExit('LVUI still owns radio state, geometry and motion')
if (root/'src/Controls/RadioGroup.h').exists():raise SystemExit('LVUI still owns radio group state')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('Radio binding directly depends on the graphics SDK')
if 'RadioButtonBinding=' not in host:raise SystemExit('Radio binding is not application-composable')
if 'class NativeRadioButton final' in host or 'set_paint_handler(' in host or 'set_input_handler(' in host:
    raise SystemExit('LVUI still owns radio drawing or input')
