from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Editing/TextEditor.h').read_text();host=(root/'src/Editing/NativeTextEditor.h').read_text()
if 'class NativeTextEditor final' in host or 'text_editor_field(' in values:
    raise SystemExit('LVUI owns text editor host or sizing')
if 'TextEditorHostPolicy' not in host or 'TextEditorBinding' not in host:
    raise SystemExit('Independent text editor binding is missing')
if any(re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',x,re.M) or 'ii::gui::' in x for x in (values,host)):
    raise SystemExit('LVUI text editor depends directly on another SDK')
