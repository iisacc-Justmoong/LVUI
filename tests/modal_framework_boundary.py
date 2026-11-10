"""The modal host and presentation belong to iiGUI; LVUI supplies independent values."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]).resolve();adapter=(root/'src/Overlays/NativeModal.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',adapter,re.M) or 'ii::gui::' in adapter:
    raise SystemExit('Modal binding directly depends on another SDK')
for token in ('ModalBinding','ModalHostModelPolicy','ModalInputValue','ModalContentFactoryValue'):
    if token not in adapter:raise SystemExit('Modal independent binding missing: '+token)
for code in ('set_popup_content_at(','builder.popover(','apply_host(','class NativeModal final'):
    if code in adapter:raise SystemExit('LVUI still implements modal visual behavior: '+code)
if 'class Modal final' in (root/'src/Overlays/Modal.h').read_text():raise SystemExit('LVUI still owns modal state and geometry')
