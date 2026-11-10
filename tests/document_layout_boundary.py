from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/'src/Editing/NativeDocument.h').read_text()
if 'DocumentLayouter' in s or 'layout_document(' in s or 'render_view(' in s or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
 raise SystemExit('LVUI still owns document drawing or a peer SDK dependency')
if 'DocumentLayoutPolicy' not in s:raise SystemExit('Document composition policy missing')
