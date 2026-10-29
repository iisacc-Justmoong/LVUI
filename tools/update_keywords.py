#!/usr/bin/env python3
"""Generate LVUI's keyword descriptors from its own checked-in registry."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / 'catalog/lvui-keywords.json'
OUTPUT = ROOT / 'src/Language/Keywords.def'
KINDS = {'view', 'modifier', 'type', 'declaration', 'literal'}
NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_]*\Z')
CORE = {'View': 'type', 'Color': 'type', 'Metric': 'type', 'import': 'declaration',
        'let': 'declaration', 'var': 'declaration', 'true': 'literal', 'false': 'literal'}


def render(source):
    data = json.loads(source)
    if (not isinstance(data, dict) or set(data) != {'schema', 'entries'} or
            type(data['schema']) is not int or data['schema'] != 1):
        raise ValueError('Invalid LVUI keyword registry schema')
    entries = data['entries']
    if not isinstance(entries, list):
        raise ValueError('Keyword entries must be a list')
    lines = {}
    for number, line in enumerate(source.splitlines(), 1):
        match = re.fullmatch(r'\s*"name": "([A-Za-z_][A-Za-z0-9_]*)",\s*', line)
        if match:
            if match[1] in lines:
                raise ValueError('Duplicate keyword name')
            lines[match[1]] = number
    names = []
    output = ['// Generated from catalog/lvui-keywords.json by tools/update_keywords.py.']
    for entry in entries:
        if (not isinstance(entry, dict) or set(entry) != {'name', 'kind', 'internal'} or
                not isinstance(entry['name'], str) or not NAME.fullmatch(entry['name']) or
                not isinstance(entry['kind'], str) or entry['kind'] not in KINDS or
                not isinstance(entry['internal'], bool)):
            raise ValueError('Invalid keyword entry')
        name = entry['name']
        if name not in lines:
            raise ValueError('Keyword source location is missing')
        names.append(name)
        output.append(f'LVUI_KEYWORD("{name}", {entry["kind"]}, "catalog/lvui-keywords.json", '
                      f'{lines[name]}, {str(entry["internal"]).lower()})')
    if len(names) != len(set(names)) or names != sorted(names):
        raise ValueError('Keyword entries must be uniquely sorted')
    if set(lines) != set(names):
        raise ValueError('Keyword source locations do not match entries')
    if any(next((entry['kind'] for entry in entries if entry['name'] == name), None) != kind
           for name, kind in CORE.items()):
        raise ValueError('The LVUI language core is missing or misclassified')
    return '\n'.join(output) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    content = render(REGISTRY.read_text(encoding='utf-8'))
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text(encoding='utf-8') != content:
            raise SystemExit('LVUI keyword descriptors are stale')
    elif not OUTPUT.is_file() or OUTPUT.read_text(encoding='utf-8') != content:
        with OUTPUT.open('w', encoding='utf-8', newline='\n') as output:
            output.write(content)
    print('LVUI keyword descriptors verified' if args.check else 'LVUI keyword descriptors updated')


if __name__ == '__main__':
    main()
