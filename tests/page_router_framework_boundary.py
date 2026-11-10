from pathlib import Path
import re,sys
source=(Path(sys.argv[1])/'src/Navigation/NativePageRouter.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',source,re.M) or 'ii::gui::' in source:
    raise SystemExit('LVUI page router directly depends on another SDK')
for token in ('NativeNavigationPageValue','FunctionalNavigationPageValue','PageRouterBinding'):
    if token not in source:raise SystemExit('Page router binding missing: '+token)
for token in ('class NativePageRouter final','builder.content_host(','set_content('):
    if token in source:raise SystemExit('LVUI owns page host implementation: '+token)
