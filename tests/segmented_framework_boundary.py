from pathlib import Path
import re,sys
root=Path(sys.argv[1])
for name in ('SegmentedControl.h','NativeSegmentedControl.h'):
    text=(root/'src/Controls'/name).read_text()
    if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',text,re.M) or 'ii::gui::' in text:
        raise SystemExit(name+' directly depends on another SDK')
    for implementation in ('class SegmentedControl final','class NativeSegmentedControl final','drawing->panels.push_back','result->panels.push_back'):
        if implementation in text:raise SystemExit(name+' still owns visual control implementation')
