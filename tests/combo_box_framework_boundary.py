from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/ComboBox.h').read_text();host=(root/'src/Controls/NativeComboBox.h').read_text()
if 'class ComboBox final' in values or 'class NativeComboBox final' in host or 'set_paint_handler(' in host:
    raise SystemExit('LVUI owns combo box graphics or state')
if 'ComboBoxModelPolicy' not in values or 'ComboBoxBinding' not in host:
    raise SystemExit('Independent combo box binding missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI combo box depends directly on another SDK')
