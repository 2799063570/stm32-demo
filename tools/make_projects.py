"""生成与参考教程目录相同的五个独立Keil工程；不重新生成业务源码。"""
from copy import deepcopy
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = [
    ('1-1 UART+DMA+空闲中断', 'UART_DMA_IDLE', ['rcc', 'gpio', 'usart', 'dma']),
    ('2-1 IIC+DMA+中断', 'IIC_DMA_IRQ', ['rcc', 'gpio', 'usart', 'dma', 'i2c']),
    ('3-1 CAN+中断', 'CAN_IRQ', ['rcc', 'gpio', 'usart', 'can']),
    ('4-1 TIM输入捕获+编码器', 'TIM_CAPTURE_ENCODER', ['rcc', 'gpio', 'usart', 'tim']),
    ('5-1 定时器PID前馈速度闭环', 'MOTOR_PID_FF', ['rcc', 'gpio', 'usart', 'dma', 'tim']),
]

def add(parent, name, value=''):
    node = ET.SubElement(parent, name)
    node.text = str(value)
    return node

def generate():
    template = ET.parse(ROOT / 'tools/TargetOption.xml').getroot()
    for folder, target_name, modules in PROJECTS:
        project_dir = ROOT / folder
        root = ET.Element('Project', {
            'xmlns:xsi': 'http://www.w3.org/2001/XMLSchema-instance',
            'xsi:noNamespaceSchemaLocation': 'project_projx.xsd',
        })
        add(root, 'SchemaVersion', '2.1')
        add(root, 'Header', '### uVision Project, (C) Keil Software')
        target = add(add(root, 'Targets'), 'Target')
        add(target, 'TargetName', target_name)
        add(target, 'ToolsetNumber', '0x4')
        add(target, 'ToolsetName', 'ARM-ADS')
        add(target, 'pCCUsed', '5060528::V5.06 update 5 (build 528)::ARMCC')
        add(target, 'uAC6', '0')
        option = deepcopy(template)
        target.append(option)

        def set_option(path, value):
            node = option.find(path)
            if node is None:
                raise ValueError(path)
            node.text = str(value)

        for tag, value in {
            'OutputDirectory': '.\\Objects\\', 'OutputName': 'Project',
            'ListingPath': '.\\Listings\\', 'CreateHexFile': 1,
            'Cpu': 'IRAM(0x20000000,0x5000) IROM(0x08000000,0x10000) CPUTYPE("Cortex-M3") CLOCK(72000000) ELITTLE',
        }.items():
            set_option('TargetCommonOption/' + tag, value)
        for tag, value in {
            'Cads/VariousControls/Define': 'STM32F10X_MD,USE_STDPERIPH_DRIVER,USE_FULL_ASSERT,HSE_VALUE=8000000',
            'Cads/VariousControls/IncludePath': '.\\Start;.\\Library;.\\User;.\\System;.\\Hardware',
            'Cads/VariousControls/MiscControls': '--c99',
            'Cads/uC99': 1, 'Cads/Optim': 2,
            'ArmAdsMisc/mOS': 0, 'ArmAdsMisc/useUlib': 1,
            'LDads/useFile': 1, 'LDads/umfTarg': 0, 'LDads/ScatterFile': '.\\User\\STM32F103C8.sct',
            'Aads/VariousControls/Define': '__MICROLIB',
        }.items():
            set_option('TargetArmAds/' + tag, value)
        groups = add(target, 'Groups')
        for group_name in ['Start', 'Library', 'User', 'System', 'Hardware']:
            group = add(groups, 'Group')
            add(group, 'GroupName', group_name)
            files = add(group, 'Files')
            paths = sorted((project_dir / group_name).glob('*'))
            if group_name == 'Start':
                paths = [p for p in paths if p.name != 'core_cm3.c']  # ARMCC使用编译器内建核心指令
            if group_name == 'Library':
                paths = [p for p in paths if p.stem in ['misc'] + ['stm32f10x_' + m for m in modules]]
            for path in paths:
                if path.suffix not in ('.c', '.h', '.s', '.sct'):
                    continue
                f = add(files, 'File')
                add(f, 'FileName', path.name)
                add(f, 'FileType', {'.c': 1, '.s': 2}.get(path.suffix, 5))
                add(f, 'FilePath', '.\\' + str(path.relative_to(project_dir)).replace('/', '\\'))
        ET.indent(root, space='  ')
        ET.ElementTree(root).write(project_dir / 'Project.uvprojx', encoding='utf-8', xml_declaration=True)

if __name__ == '__main__':
    generate()
