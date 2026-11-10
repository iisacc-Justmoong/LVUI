from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Controls/Stepper.h').read_text();host=(root/'src/Controls/NativeStepper.h').read_text()
if 'class Stepper final' in values or any(v in host for v in ('class NativeStepper final','set_paint_handler(','set_input_handler(')):
    raise SystemExit('LVUI still owns stepper visual behavior')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('LVUI stepper directly depends on another SDK')
if 'StepperBinding' not in host or 'StepperModelPolicy' not in host:raise SystemExit('Stepper independent binding missing')
