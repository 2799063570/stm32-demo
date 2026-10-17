"""检查五个真实构建产物的HEX、内存范围与中断向量，不模拟外设。"""
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET
from make_projects import ROOT, PROJECTS

EXPECTED = [
    {'USART1_IRQHandler': (53, 'serial'), 'DMA1_Channel4_IRQHandler': (30, 'serial'), 'DMA1_Channel5_IRQHandler': (31, 'serial')},
    {'I2C2_EV_IRQHandler': (49, 'iic'), 'I2C2_ER_IRQHandler': (50, 'iic'), 'DMA1_Channel4_IRQHandler': (30, 'iic'), 'DMA1_Channel5_IRQHandler': (31, 'iic')},
    {'USB_HP_CAN1_TX_IRQHandler': (35, 'can'), 'USB_LP_CAN1_RX0_IRQHandler': (36, 'can')},
    {'TIM2_IRQHandler': (44, 'ic')},
    {'TIM2_IRQHandler': (44, 'timer'), 'USART1_IRQHandler': (53, 'serial'), 'DMA1_Channel4_IRQHandler': (30, 'serial'), 'DMA1_Channel5_IRQHandler': (31, 'serial')},
]

def read_hex(path):
    memory, base, eof = {}, 0, False
    for line in path.read_text(encoding='ascii').splitlines():
        assert line.startswith(':') and not eof, f'{path}: invalid record'
        data = bytes.fromhex(line[1:])
        assert len(data) == data[0] + 5 and sum(data) % 256 == 0, f'{path}: checksum'
        count, kind = data[0], data[3]
        address = int.from_bytes(data[1:3], 'big')
        if kind == 0:
            for offset, value in enumerate(data[4:4 + count]):
                location = base + address + offset
                assert 0x08000000 <= location < 0x08010000, 'FLASH exceeds C8 64KiB'
                assert location not in memory, 'overlapping HEX records'
                memory[location] = value
        elif kind == 4:
            assert count == 2
            base = int.from_bytes(data[4:6], 'big') << 16
        elif kind == 1:
            assert count == 0
            eof = True
        else:
            assert kind == 5 and count == 4, 'unsupported HEX record'
    assert memory and eof
    return memory

def symbol(text, name, owner):
    match = re.search(r'^\s+' + re.escape(name) + r'\s+(0x\w+)\s+Thumb Code\s+\d+\s+([^\s]+)', text, re.M)
    assert match and match[2].lower().startswith(owner.lower() + '.o('), f'ISR missing or still weak: {name}'
    return int(match[1], 16)

def check(index, folder):
    project = ROOT / folder
    xml = ET.parse(project / 'Project.uvprojx')
    for file in xml.findall('./Targets/Target/Groups/Group/Files/File/FilePath'):
        relative = Path(file.text.replace('\\', '/'))
        target = (project / relative).resolve()
        assert target.is_relative_to(project) and target.is_file(), f'Invalid project source: {file.text}'
    memory = read_hex(project / 'Objects/Project.hex')
    text = (project / 'Listings/Project.map').read_text(encoding='utf-8', errors='replace')
    def vector(slot):
        return struct.unpack('<I', bytes(memory[0x08000000 + slot * 4 + offset] for offset in range(4)))[0]
    assert 0x20000000 < vector(0) <= 0x20005000 and vector(0) % 8 == 0, 'invalid stack'
    handlers = {'Reset_Handler': (1, 'startup_stm32f10x_md'), 'SysTick_Handler': (15, 'stm32f10x_it'), **EXPECTED[index]}
    for name, (slot, owner) in handlers.items():
        location = symbol(text, name, owner)
        assert location & 1 and 0x08000000 <= location < 0x08010000
        assert vector(slot) == location, f'Wrong vector binding: {name}'
    symbol(text, 'main', 'main')
    for region, expected in [('ER_IROM1', (0x08000000, 65536)), ('RW_IRAM1', (0x20000000, 20480))]:
        match = re.search(r'Execution Region ' + region + r' \((?:Base|Exec base): (0x\w+)(?:, Load base: 0x\w+)?, Size: (0x\w+), Max: (0x\w+)', text)
        assert match, f'Missing memory region {region}'
        base, size, maximum = (int(value, 16) for value in match.groups())
        assert (base, maximum) == expected and size <= maximum
        if region == 'RW_IRAM1':
            ram = size
    log = (project / 'Objects/Build.log').read_text(encoding='utf-8', errors='replace')
    assert '0 Error(s), 0 Warning(s).' in log, 'Build log has warnings/errors'
    print(f'{folder}: PASS  FLASH={max(memory) - 0x08000000 + 1} B  RAM={ram} B')

if __name__ == '__main__':
    for index, (folder, _, _) in enumerate(PROJECTS):
        check(index, folder)
