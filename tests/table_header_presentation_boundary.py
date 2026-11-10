from pathlib import Path
import re,sys
header=(Path(sys.argv[1])/'src/Collections/NativeTableHeaderPresentation.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',header,re.M) or 'ii::gui::' in header:
    raise SystemExit('Table header policy directly depends on the graphics SDK')
if 'class NativeTableHeaderPresentation final' in header or 'drawing.images.push_back' in header:
    raise SystemExit('LVUI still owns table header rendering')
