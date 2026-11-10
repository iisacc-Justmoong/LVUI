from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/CheckBox.h').read_text();host=(root/'src/Controls/NativeCheckBox.h').read_text()
if 'class CheckBox final' in values: raise SystemExit('LVUI still owns CheckBox state, geometry and motion')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('CheckBox binding directly depends on the graphics SDK')
if 'CheckBoxBinding=' not in host: raise SystemExit('CheckBox binding is not application-composable')
if 'class NativeCheckBox final' in host or 'set_paint_handler(' in host or 'set_interaction_handler(' in host:
    raise SystemExit('LVUI still owns CheckBox drawing or input')
