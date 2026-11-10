"""An ImageIO contract must not import image, I/O or networking SDKs."""
from pathlib import Path
import re
import sys
root=Path(sys.argv[1])
package=(root/'cmake/LVUIConfig.cmake.in').read_text()
project=(root/'CMakeLists.txt').read_text()
helper=(root/'cmake/LVUIAot.cmake').read_text()
for package_name in ('iiGUI','iiNetwork','iiFileProviderNative','iiVisualCodec'):
    if re.search(r'find_dependency\s*\(\s*'+package_name+r'\b',package,re.I):
        raise SystemExit('LVUI package directly requires '+package_name)
for match in re.finditer(r'target_link_libraries\s*\(\s*LVUIImageIO\b([^)]*)\)',project,re.I|re.S):
    if re.search(r'\b(?:iiGUI|iiNetwork|iiFileProvider|iiVisualCodec)::',match.group(1),re.I):
        raise SystemExit('LVUIImageIO directly links another SDK')
if re.search(r'find_package\s*\(\s*iiGUI\b',helper,re.I):
    raise SystemExit('LVUI application helper implicitly discovers iiGUI')
