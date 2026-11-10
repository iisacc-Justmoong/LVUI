from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/ToggleSwitch.h').read_text();host=(root/'src/Controls/NativeToggleSwitch.h').read_text()
if 'class ToggleSwitch final' in values:raise SystemExit('LVUI still owns the ToggleSwitch state and motion model')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('Toggle binding directly depends on the graphics SDK')
if 'ToggleSwitchBinding=' not in host:raise SystemExit('Toggle binding is not application-composable')
if 'class NativeToggleSwitch final' in host or 'set_paint_handler(' in host or 'set_input_handler(' in host:
    raise SystemExit('LVUI still owns ToggleSwitch drawing or input')
