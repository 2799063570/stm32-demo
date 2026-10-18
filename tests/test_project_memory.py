"""回归：Target页IROM写成SRAM时，即使已有HEX正确也必须拒绝。"""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from make_projects import ROOT, PROJECTS, set_memory_layout
from check_projects import check_target_memory


class TargetMemoryTests(unittest.TestCase):
    def setUp(self):
        self.option = ET.parse(ROOT / 'tools/TargetOption.xml').getroot()

    def test_template_and_all_projects(self):
        check_target_memory(self.option, 'template')
        for folder, _, _ in PROJECTS:
            with self.subTest(project=folder):
                for option in ET.parse(ROOT / folder / 'Project.uvprojx').findall('./Targets/Target/TargetOption'):
                    check_target_memory(option, folder)

    def test_original_bug_rejected_and_generator_repairs(self):
        memory = self.option.find('TargetArmAds/ArmAdsMisc/OnChipMemories/OCR_RVCT4')
        memory.find('StartAddress').text = '0x20000000'
        memory.find('Size').text = '0x5000'
        before_linker = deepcopy(self.option.find('TargetArmAds/LDads'))
        with self.assertRaisesRegex(AssertionError, 'OCR_RVCT4 incorrect'):
            check_target_memory(self.option, 'broken')
        set_memory_layout(self.option)
        check_target_memory(self.option, 'repaired')
        self.assertEqual(ET.tostring(before_linker), ET.tostring(self.option.find('TargetArmAds/LDads')))

    def test_ram_and_layout_changes_rejected(self):
        for path, value in [
            ('TargetArmAds/ArmAdsMisc/OnChipMemories/OCR_RVCT9/Size', '0x10000'),
            ('TargetArmAds/ArmAdsMisc/Im1Chk', '0'),
            ('TargetArmAds/LDads/umfTarg', '1'),
            ('TargetCommonOption/Cpu', 'IRAM(0x20000000,0x5000) IROM(0x20000000,0x5000)'),
        ]:
            with self.subTest(path=path):
                option = deepcopy(self.option)
                option.find(path).text = value
                with self.assertRaises(AssertionError):
                    check_target_memory(option, 'broken')


if __name__ == '__main__':
    unittest.main()
