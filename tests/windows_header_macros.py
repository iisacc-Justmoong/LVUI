"""Guard authored headers against calls expanded by Windows min/max macros."""

from pathlib import Path
import re
import sys

PATTERN = re.compile(
    r"\bstd::(?:min|max)\s*\("
    r"|\bstd::numeric_limits<[^;\n]*?>::(?:min|max)\s*\("
    r"|\bii::runtime::Clock::(?:duration|time_point)::(?:min|max)\s*\("
)

root = Path(sys.argv[1])
failures = []
for path in sorted(root.rglob("*.h")):
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if PATTERN.search(line):
            failures.append(f"{path.relative_to(root)}:{number}: {line.strip()}")
if failures:
    raise SystemExit("Windows min/max macro-sensitive calls:\n" + "\n".join(failures))
