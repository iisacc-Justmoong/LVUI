"""The button host is supplied by iiGUI; LVUI supplies independent policy."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]).resolve();adapter=(root/'src/Controls/NativeButton.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',adapter,re.M) or 'ii::gui::' in adapter:
    raise SystemExit('LVUI button directly depends on a graphics SDK')
for name in ('ButtonBinding','ButtonHostModelPolicy','ButtonInputValue','NativeIconCache<Decoder>'):
    if name not in adapter:raise SystemExit('LVUI button binding missing: '+name)
for code in ('set_paint_handler(','set_interaction_handler(','request_frames(','class NativeButton final'):
    if code in adapter:raise SystemExit('LVUI still implements button visual behavior: '+code)
