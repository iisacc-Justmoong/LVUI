from pathlib import Path
import re,sys
source=(Path(sys.argv[1])/'src/Style/NativeMaterialGradient.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI material gradient directly depends on another SDK')
if 'MaterialGradientPolicy' not in source or 'MaterialGradientBinding' not in source:
    raise SystemExit('Independent material gradient policy missing')
if 'class NativeMaterialGradient final' in source or 'rgba_image(' in source:
    raise SystemExit('LVUI still owns material rasterization')
