from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/'src/Controls/Slider.h').read_text()
if 'class Slider final' in s or 'resolved_value(' in s:
    raise SystemExit('LVUI owns slider range or geometry')
if 'SliderModelPolicy' not in s or 'SliderBinding' not in s:
    raise SystemExit('Independent slider model binding is missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
    raise SystemExit('LVUI slider values depend directly on another SDK')
