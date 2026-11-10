from pathlib import Path
import re,sys
header=(Path(sys.argv[1])/'src/Collections/NativeTableViewGroup.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',header,re.M) or 'ii::gui::' in header:
    raise SystemExit('Table view binding directly depends on the graphics SDK')
if 'class NativeTableViewGroup final' in header or 'set_keyed_content(' in header:
    raise SystemExit('LVUI still owns table view mounting')
