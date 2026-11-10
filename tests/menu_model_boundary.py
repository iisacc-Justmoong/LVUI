from pathlib import Path
import re,sys
source=(Path(sys.argv[1])/'src/Overlays/Menu.h').read_text()
if 'class Menu final' in source:raise SystemExit('LVUI owns menu state and geometry')
if 'MenuModelPolicy' not in source:raise SystemExit('Independent menu design policy missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI menu values depend directly on another SDK')
