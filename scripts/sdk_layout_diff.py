#!/usr/bin/env python3
"""Offline comparison of SDK AIOHeader.hpp files. Does not access game processes."""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

OBJECT = re.compile(r'^\s*//\s*Object:\s*(.+?)\s*$')
SIZE = re.compile(r'^\s*//\s*Size:\s*(0x[0-9a-fA-F]+)')
MEMBER = re.compile(r'^\s*(.*?)\s+([A-Za-z_]\w*(?:\[[^\]]+\])?)(?:\s*:\s*(\d+))?\s*;\s*//\s*0x([0-9a-fA-F]+)\(0x([0-9a-fA-F]+)\)(?:,\s*Mask\((0x[0-9A-Fa-f]+)\))?')
DECL = re.compile(r'^\s*(?:struct|class)\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?)\b')


def load_header(path):
    result, active, size, declared = {}, None, None, False
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        m = OBJECT.match(line)
        if m and not declared:
            active, size = m.group(1), None
            continue
        if active is None:
            continue
        m = SIZE.match(line)
        if m and not declared:
            size = int(m.group(1), 16)
            continue
        if not declared:
            if DECL.match(line):
                declared = True
                result[active] = {'size': size, 'members': {}}
            continue
        if line.strip() == '};':
            active, declared = None, False
            continue
        m = MEMBER.match(line)
        if m:
            dtype, name, bit_width, offset, width, bit_mask = m.groups()
            if name.startswith(('Pad_', 'BitPad_')):
                continue
            result[active]['members'][name] = {
                'type': re.sub(r'\s+', ' ', dtype).strip(),
                'offset': int(offset, 16), 'size': int(width, 16),
                'bit_width': int(bit_width) if bit_width is not None else None,
                'bit_mask': int(bit_mask, 16) if bit_mask is not None else None,
            }
    return result


def compare(old, new):
    changes = []
    for obj in sorted(set(old) | set(new)):
        before, after = old.get(obj), new.get(obj)
        if before is None or after is None:
            changes.append({'object': obj, 'member': '',
                            'status': 'new_object' if after else 'removed_object',
                            'old': before, 'new': after})
            continue
        if before['size'] != after['size']:
            changes.append({'object': obj, 'member': '(object size)',
                            'status': 'changed', 'old': before['size'], 'new': after['size']})
        for name in sorted(set(before['members']) | set(after['members'])):
            a, b = before['members'].get(name), after['members'].get(name)
            if a != b:
                changes.append({'object': obj, 'member': name,
                                'status': 'new_member' if a is None else 'removed_member' if b is None else 'changed',
                                'old': a, 'new': b})
    return changes


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--old', type=Path, required=True, help='Old SDK AIOHeader.hpp')
    p.add_argument('--new', type=Path, required=True, help='New SDK AIOHeader.hpp')
    p.add_argument('--json', type=Path)
    p.add_argument('--csv', type=Path)
    args = p.parse_args()
    for path in (args.old, args.new):
        if not path.is_file():
            p.error('file not found: ' + str(path))
    before, after = load_header(args.old), load_header(args.new)
    if not before or not after:
        print('ERROR: an input contains no parseable SDK objects', file=sys.stderr)
        return 2
    changes = compare(before, after)
    result = {'old_file': str(args.old), 'new_file': str(args.new),
              'old_objects': len(before), 'new_objects': len(after),
              'changed_entries': len(changes),
              'notice': 'Offline declaration comparison only; no runtime offset validation.',
              'changes': changes}
    print(f'Old objects: {len(before)}; New objects: {len(after)}; Changed entries: {len(changes)}')
    for item in changes[:100]:
        print(f"{item['status']:14s} {item['object']} :: {item['member']}")
    if len(changes) > 100:
        print(f'... {len(changes)-100} more entries; use --json or --csv')
    if args.json:
        args.json.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
    if args.csv:
        with args.csv.open('w', encoding='utf-8-sig', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['object', 'member', 'status', 'old', 'new'])
            for row in changes:
                writer.writerow([row['object'], row['member'], row['status'],
                                 json.dumps(row['old'], ensure_ascii=False),
                                 json.dumps(row['new'], ensure_ascii=False)])
    return 0


if __name__ == '__main__':
    sys.exit(main())
