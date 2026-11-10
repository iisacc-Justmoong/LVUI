from pathlib import Path
import re,sys
header=(Path(sys.argv[1])/'src/Controls/NativeButtonMethods.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',header,re.M) or 'ii::gui::' in header:
    raise SystemExit('Button methods directly depend on the graphics SDK')
for token in ('ButtonEventValue','ButtonMethodValue','ButtonMethodRegistryValue','collect_button_methods'):
    if token not in header:raise SystemExit('Independent button contract missing: '+token)
