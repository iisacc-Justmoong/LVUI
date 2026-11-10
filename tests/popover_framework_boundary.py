from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Overlays/Popover.h').read_text();host=(root/'src/Overlays/NativePopover.h').read_text()
if 'class Popover final' in values or 'class NativePopover final' in host or 'builder.popover(' in host:
    raise SystemExit('LVUI owns popover graphics or state')
if 'PopoverModelPolicy' not in values or 'PopoverBinding' not in host:
    raise SystemExit('Independent popover contract missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI popover depends directly on another SDK')
