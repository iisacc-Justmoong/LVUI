from pathlib import Path
import sys
root=Path(sys.argv[1])
if (root/'src/Editing/NativeEditorCanvas.h').exists():
 raise SystemExit('LVUI still owns editor canvas host')
if 'src/Editing/NativeEditorCanvas.h' in (root/'CMakeLists.txt').read_text():
 raise SystemExit('LVUI still exports editor canvas host')
