from pathlib import Path
import re,sys
root=Path(sys.argv[1]);p=root/'src/Editing/NativeEditorTimeline.h'
if not p.exists():raise SystemExit('Timeline design binding missing')
s=p.read_text()
if 'class NativeEditorTimeline final' in s or 'set_paint_handler(' in s or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
 raise SystemExit('LVUI owns timeline graphics or depends on peer SDK')
if 'editor_timeline_palette' not in s:raise SystemExit('LVUI timeline palette binding missing')
