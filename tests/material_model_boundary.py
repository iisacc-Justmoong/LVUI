from pathlib import Path
import re,sys
source=(Path(sys.argv[1])/'src/Style/Material.h').read_text()
if 'class Material final' in source:raise SystemExit('LVUI still owns material graphics and motion')
if 'MaterialModelPolicy' not in source:raise SystemExit('Independent material model policy missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI material values depend directly on another SDK')
