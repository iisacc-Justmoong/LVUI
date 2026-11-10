"""Named icons are iiGUI widgets and no LVUI icon widget is installed."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
if (root/'src/Media/NativeIcon.h').exists():
    raise SystemExit('LVUI still owns the native icon widget')
source=(root/'src/Compiler/Compiler.cpp').read_text()
if 'ii::gui::IconView::create(' not in source or '<iiGUI/NativeIconView.h>' not in source:
    raise SystemExit('Generated icons do not use the iiGUI host')
if '<LVUI/NativeIcon.h>' in source or 'lvui::native::NativeIcon::create(' in source:
    raise SystemExit('Generated icons still depend on the LVUI host')
