#!/usr/bin/env python3
"""Offline parsing tests for SDK headers; no live process access."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sdk_layout_diff import load_header, compare

OLD = """// Object: ScriptStruct CoreUObject.QAExample
// Size: 0x18 (Inherited: 0x0)
struct FQAExample
{
    uint8_t bEnabled : 1; // 0x0(0x1), Mask(0x1)
    uint8_t BitPad_0x0_1 : 7; // 0x0(0x1)
    struct FName Name; // 0x8(0x8)
    uint8_t Pad_0x10[0x8]; // 0x10(0x8)
};
"""
NEW = """// Object: ScriptStruct CoreUObject.QAExample
// Size: 0x20 (Inherited: 0x0)
struct FQAExample
{
    uint8_t bEnabled : 2; // 0x0(0x1), Mask(0x3)
    uint8_t BitPad_0x0_2 : 6; // 0x0(0x1)
    struct FName Name; // 0x10(0x8)
    uint8_t Pad_0x18[0x8]; // 0x18(0x8)
};
"""

class TestSdkLayoutDiff(unittest.TestCase):
    def test_bitfields_and_padding(self):
        with tempfile.TemporaryDirectory() as d:
            old = Path(d) / 'old.hpp'
            new = Path(d) / 'new.hpp'
            old.write_text(OLD)
            new.write_text(NEW)
            left, right = load_header(old), load_header(new)
            self.assertEqual(len(left), 1)
            key = 'ScriptStruct CoreUObject.QAExample'
            self.assertEqual(left[key]['members']['bEnabled']['bit_width'], 1)
            self.assertEqual(right[key]['members']['bEnabled']['bit_mask'], 3)
            self.assertEqual(left[key]['members']['Name']['offset'], 8)
            self.assertFalse(any(x.startswith(('Pad_', 'BitPad_')) for x in left[key]['members']))
            changes = compare(left, right)
            self.assertEqual({x['member'] for x in changes}, {'(object size)', 'bEnabled', 'Name'})
    def test_no_changes_for_same_file(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'old.hpp'
            p.write_text(OLD)
            self.assertEqual(compare(load_header(p), load_header(p)), [])

if __name__ == '__main__':
    unittest.main()
