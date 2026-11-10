"""The LVUI color contract must not make consumers discover another SDK."""
from pathlib import Path
import re
import sys

root=Path(sys.argv[1])
package=(root/'cmake/LVUIConfig.cmake.in').read_text()
project=(root/'CMakeLists.txt').read_text()
if re.search(r'find_dependency\s*\(\s*iiColorManager\b',package,re.I):
    raise SystemExit('LVUI package directly requires iiColorManager')
for match in re.finditer(r'target_link_libraries\s*\(\s*LVUIColor\b([^)]*)\)',project,re.I|re.S):
    if re.search(r'iiColorManager::',match.group(1),re.I):
        raise SystemExit('LVUIColor directly links iiColorManager')
