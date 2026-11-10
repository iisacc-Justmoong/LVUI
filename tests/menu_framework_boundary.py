from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Overlays/NativeMenu.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('LVUI menu directly depends on another SDK')
for token in ('MenuBinding','MenuSectionPortValue','MenuRegistrationPortValue','MenuSectionAccessValue'):
    if token not in host:raise SystemExit('Independent menu binding missing: '+token)
for token in ('class NativeMenu final','set_paint_handler(','set_input_handler(','builder.popover('):
    if token in host:raise SystemExit('LVUI still owns menu graphics or input: '+token)
