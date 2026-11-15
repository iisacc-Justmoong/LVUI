from pathlib import Path
import sys

for name in sys.argv[1:]:
    source = Path(name).read_text(encoding="utf-8")
    if source.count("auto visibility_finalizers =") != 1:
        raise SystemExit(f"Only the scope that declares visible notifications should allocate retirement state: {name}")
