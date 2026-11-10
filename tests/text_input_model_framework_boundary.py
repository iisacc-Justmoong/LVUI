from pathlib import Path
import re,sys
p=Path(sys.argv[1])/'src/Editing/TextInput.h';s=p.read_text()
if 'class TextInput final' in s or 'struct HistoryEntry final' in s:
    raise SystemExit('LVUI owns input editing state or history')
if 'TextInputModelPolicy' not in s or 'TextInputBinding' not in s:
    raise SystemExit('Independent text input binding is missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
    raise SystemExit('LVUI text input contract depends directly on another SDK')
