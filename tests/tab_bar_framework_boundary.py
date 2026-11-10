from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/'src/Controls/NativeTabBar.h').read_text()
if 'class NativeTabBar final' in s or 'set_input_handler(' in s or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
 raise SystemExit('LVUI owns tab bar host or a peer SDK dependency')
if 'TabBarBinding' not in s or 'TabSectionPortValue' not in s:raise SystemExit('Independent tab bar binding missing')
