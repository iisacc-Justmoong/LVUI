from pathlib import Path
import re,sys
source=(Path(sys.argv[1])/'src/Style/NativeMaterial.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI material host directly depends on another SDK')
for token in ('MaterialBinding','MaterialSourcesValue','MaterialHostPolicy'):
    if token not in source:raise SystemExit('Independent material host binding missing: '+token)
for token in ('class NativeMaterial final','set_paint_handler(','request_frames(','builder.zstack('):
    if token in source:raise SystemExit('LVUI still owns material widget behavior: '+token)
