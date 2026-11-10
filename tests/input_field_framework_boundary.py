from pathlib import Path
import re,sys
p=Path(sys.argv[1])/'src/Editing/NativeInputField.h';s=p.read_text()
if 'class NativeInputField final' in s or 'set_paint_handler(' in s:
    raise SystemExit('LVUI owns input field rendering or state')
if 'InputFieldHostPolicy' not in s or 'InputFieldBinding' not in s:
    raise SystemExit('Independent input field binding is missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
    raise SystemExit('LVUI input field depends directly on another SDK')
