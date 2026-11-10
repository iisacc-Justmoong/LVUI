"""LVUI's icon asset adapter may depend on Core, never on the GUI SDK."""
from pathlib import Path
import re
import sys
root=Path(sys.argv[1])
for name in ('NativeAuthoredIcons.h', 'NativeIconCache.h'):
    source=(root/'src/Media'/name).read_text()
    if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
        raise SystemExit(f'{name} directly depends on another SDK')
    if 'template<class Decoder>' not in source:
        raise SystemExit(f'{name} does not accept an application-supplied decoder')

source=(root/'src/Controls/NativeAvatarAssets.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('Avatar design assets directly depend on another SDK')
