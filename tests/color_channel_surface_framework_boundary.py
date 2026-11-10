from pathlib import Path
import re,sys
p=Path(sys.argv[1])/'src/Color/ColorChannelSurface.h';s=p.read_text()
if 'class ColorChannelSurface final' in s or '.rgba.resize(' in s:
    raise SystemExit('LVUI owns color channel rasterization')
if 'ColorChannelSurfacePolicy' not in s or 'ColorChannelSurfaceBinding' not in s:
    raise SystemExit('Independent color channel binding is missing')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',s,re.M) or 'ii::gui::' in s:
    raise SystemExit('LVUI color channel contract depends directly on another SDK')
