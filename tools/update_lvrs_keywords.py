#!/usr/bin/env python3
"""Audit LVRS sources and maintain LVUI's dependency-free keyword snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/lvrs-components.json"
DEFINITIONS = ROOT / "src/LVUI/Keywords.def"
DOCUMENT = ROOT / "docs/ReservedWords.md"
VIEW_BASES = {
    "Item", "Rectangle", "FocusScope", "Canvas", "Row", "Column", "ColumnLayout",
    "Loader", "ToolBar", "Controls.AbstractButton", "Controls.ApplicationWindow",
    "Controls.Switch", "Controls.Popup", "Controls.Control", "Controls.ToolTip",
    "T.Slider", "QtQuickWindow.Window", "QQuickItem", "QQuickPaintedItem", "QQuickWindow",
}
NONVISUAL_BASES = {"QtObject", "QObject", "QAbstractListModel"}
MODIFIER_BASES = {"Scale", "Behavior"}
CORE = {"View": "type", "import": "declaration", "let": "declaration",
        "var": "declaration", "true": "literal", "false": "literal"}


def uncomment(text):
    # Keep strings and newlines intact so declarations in comments are ignored
    # without changing source coordinates or recognizing comment delimiters in strings.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: re.sub(r'[^\n]', ' ', m[0])
                  if m[0].startswith(('//', '/*')) else m[0], text)


def audit(source):
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    files, declarations, texts = [], [], {}
    tracked = subprocess.check_output(["git", "-C", str(source), "ls-files", "-z",
                                       "--cached", "--others", "--exclude-standard", "--", "src"])
    for relative in sorted(set(p for p in tracked.decode('utf-8').split('\0') if p)):
        path = source / relative
        raw = path.read_bytes()
        normalized = raw.decode('utf-8').replace('\r\n', '\n').replace('\r', '\n')
        files.append({"path": relative, "sha256": hashlib.sha256(normalized.encode('utf-8')).hexdigest()})
        text = uncomment(normalized)
        texts[relative] = text
        code = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|`(?:\\.|[^`\\])*`',
                      lambda m: re.sub(r'[^\n]', ' ', m[0]), text)
        if path.suffix == '.qml':
            match = re.search(r'(?m)^([A-Za-z_][\w.]*)\s*\{', code)
            if not match:
                raise ValueError(f'Cannot determine QML root: {relative}')
            declarations.append({"name": path.stem, "base": match[1], "path": relative,
                                 "line": text.count('\n', 0, match.start()) + 1, "origin": "qml"})
            for match in re.finditer(r'\bcomponent\s+([A-Za-z_]\w*)\s*:\s*([\w.]+)\s*\{', code):
                declarations.append({"name": match[1], "base": match[2], "path": relative,
                                     "line": text.count('\n', 0, match.start()) + 1, "origin": "qml-inline"})
        elif path.suffix in {'.h', '.hpp'}:
            classes = list(re.finditer(r'\bclass\s+(?:LVRS_EXPORT\s+)?(\w+)\s*(?:final\s*)?:\s*public\s+(\w+)', code))
            for match in re.finditer(r'\bQML_NAMED_ELEMENT\s*\(\s*(\w+)\s*\)', code):
                owner = next((c for c in reversed(classes) if c.start() < match.start()), None)
                if owner is None:
                    raise ValueError(f'Cannot determine registered C++ base: {relative}')
                declarations.append({"name": match[1], "base": owner[2], "path": relative,
                                     "line": text.count('\n', 0, match.start()) + 1, "origin": "cpp"})

    by_name = {}
    for declaration in declarations:
        by_name.setdefault(declaration['name'], []).append(declaration)

    def classify(base, active=()):
        if base in VIEW_BASES: return 'view'
        if base in NONVISUAL_BASES: return 'nonvisual'
        if base in MODIFIER_BASES: return 'modifier'
        if base in active or base not in by_name:
            raise ValueError(f'Unresolved or cyclic component base: {base}')
        kinds = {classify(d['base'], active + (base,)) for d in by_name[base]}
        if len(kinds) != 1: raise ValueError(f'Ambiguous component base: {base}')
        return kinds.pop()

    internal = set()
    cmake = '\n'.join(value for key, value in texts.items() if key.endswith('CMakeLists.txt'))
    for block in re.finditer(r'set_source_files_properties\((.*?)\)', cmake, re.S):
        if re.search(r'QT_QML_INTERNAL_TYPE\s+TRUE', block[1]):
            internal.update(re.findall(r'src/[\w/]+\.qml', block[1]))
    for declaration in declarations:
        declaration['kind'] = classify(declaration['base'])
        declaration['internal'] = declaration['origin'] == 'qml-inline' or declaration['path'] in internal
        if declaration['origin'] == 'qml' and declaration['path'] not in cmake:
            raise ValueError(f'QML source missing from build registration: {declaration["path"]}')
    return {"schema": 1, "source_repository": "https://github.com/iisacc-Justmoong/LVRS",
            "source_revision": revision, "files": files, "declarations": declarations}


def words(snapshot):
    result = {name: {"name": name, "kind": kind, "path": '', "line": 0, "internal": False}
              for name, kind in CORE.items()}
    for entry in sorted(snapshot['declarations'], key=lambda d: (d['internal'], d['path'], d['line'])):
        if entry['kind'] == 'nonvisual': continue
        name = entry['name']
        if name in CORE: raise ValueError(f'LVRS collides with core keyword: {name}')
        if name in result:
            if result[name]['kind'] != entry['kind']: raise ValueError(f'Conflicting keyword: {name}')
            continue
        result[name] = entry
    return [result[key] for key in sorted(result)]


def render(snapshot):
    entries = words(snapshot)
    definitions = '// Generated by tools/update_lvrs_keywords.py; do not edit by hand.\n'
    for entry in entries:
        definitions += 'LVUI_KEYWORD("{name}", {kind}, "{path}", {line}, {internal})\n'.format(
            **{**entry, 'internal': str(entry['internal']).lower()})
    views = sum(entry['kind'] == 'view' for entry in entries)
    modifiers = sum(entry['kind'] == 'modifier' for entry in entries)
    document = ('# 예약어와 LVRS 시각 컴포넌트\n\n'
                '이 문서는 `tools/update_lvrs_keywords.py`로 소스 조사 결과에서 생성한다.\n'
                f'조사 기준 LVRS 커밋은 `{snapshot["source_revision"]}`이며, '
                f'`src/`의 {len(snapshot["files"])}개 파일을 읽고 LF 개행으로 정규화한 SHA-256을 기록했다.\n'
                f'시각 컴포넌트의 고유 이름 {views}개와 시각 효과·동작 보조 예약어 {modifiers}개를 포함한다.\n\n'
                '## 실제 언어 구현\n\n'
                '`LVUI/Language.h`의 `keywords`, `find_keyword`, `is_reserved_identifier`, '
                '`tokenize`, `parse_view`가 이 목록을 사용한다.\n'
                '시각 컴포넌트 이름은 `View` 생성 구문으로 파싱하고, `{}` 안의 자식 뷰를 소유 트리에 보관한다.\n'
                '`let`·`var` 이름과 import 바인딩에 예약어를 사용하면 진단한다. '
                '예약은 대소문자를 구분하며 문자열과 주석의 텍스트에는 적용하지 않는다.\n'
                'QML 내부 타입과 인라인 컴포넌트도 포함하며, 같은 이름의 여러 원본 선언은 조사 파일에 모두 보관한다.\n'
                '`InteractionMotion`, `SpringBehavior`, `StateColorBehavior`는 시각 보조 예약어이며 '
                '자식 `View` 생성자로 사용하지 않는다.\n'
                '이 목록은 컴파일 단계의 어휘·구문 계약이다. '
                '현재 지원 컴포넌트의 C++23 AOT와 iiGUI 자체 위젯 마운트는 '
                '[LVRS 네이티브 구현](LVRSNative.md)에 별도로 명시한다. '
                '예약어 등록만으로 모든 속성·상태·동작이 구현되지는 않는다.\n\n'
                '## 예약어 목록\n\n| 예약어 | 분류 | 원본 | 내부 타입 |\n|---|---|---|---|\n')
    labels = {'view': 'View', 'modifier': '시각 보조', 'type': '타입',
              'declaration': '선언', 'literal': '리터럴'}
    for entry in entries:
        location = f'`{entry["path"]}:{entry["line"]}`' if entry['path'] else 'LVUI 언어'
        document += f'| `{entry["name"]}` | {labels[entry["kind"]]} | {location} | {"예" if entry["internal"] else "아니오"} |\n'
    document += '\n## 비시각 객체\n\n다음 이름은 화면 트리의 `View`가 아닌 서비스·모델·상태 객체이므로 시각 컴포넌트 예약어에서 제외한다.\n\n'
    for entry in snapshot['declarations']:
        if entry['kind'] == 'nonvisual':
            document += f'- `{entry["name"]}`: `{entry["path"]}:{entry["line"]}`, 기반 `{entry["base"]}`.\n'
    document += ('\n## 목록 갱신과 검증\n\n'
                 '```sh\npython3 tools/update_lvrs_keywords.py --lvrs-root ../LVRS\n'
                 'python3 tools/update_lvrs_keywords.py --lvrs-root ../LVRS --check\n'
                 'python3 tools/update_lvrs_keywords.py --snapshot-only --check\n```\n\n'
                 '일반 빌드와 설치 소비자는 LVRS·Qt·이 스크립트를 요구하지 않는다. '
                 '원본 조사 테스트에서 새 파일·등록·상속·내용 변경이 발견되면 목록을 다시 조사해야 한다. '
                 '내용이 같은 출력은 파일과 수정 시각을 보존한다. 비시각 소스의 내용만 바뀌었을 때 '
                 '같은 예약어 헤더를 다시 써서 하위 C++ 모듈을 재컴파일시키지 않는다.\n')
    return {DEFINITIONS: definitions, DOCUMENT: document}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvrs-root', type=Path, default=ROOT.parent / 'LVRS')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--snapshot-only', action='store_true')
    args = parser.parse_args()
    snapshot = json.loads(CATALOG.read_text()) if args.snapshot_only else audit(args.lvrs_root.resolve())
    outputs = render(snapshot)
    outputs[CATALOG] = json.dumps(snapshot, ensure_ascii=False, indent=2) + '\n'
    for path, content in outputs.items():
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit(f'Stale keyword inventory: {path.relative_to(ROOT)}')
        elif not path.exists() or path.read_text() != content:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
    entries = words(snapshot)
    print(f'{len(snapshot["files"])} source files; {len(snapshot["declarations"])} declarations; '
          f'{sum(e["kind"] == "view" for e in entries)} View keywords; '
          f'{sum(e["kind"] == "modifier" for e in entries)} visual modifiers; '
          f'{len(entries)} total reserved words: {"verified" if args.check else "updated"}')


if __name__ == '__main__':
    main()
