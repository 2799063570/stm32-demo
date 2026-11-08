"""生成与参考教程目录相同的独立Keil工程；不重新生成业务源码。"""
from copy import deepcopy
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ET.register_namespace('xsi', 'http://www.w3.org/2001/XMLSchema-instance')
PROJECTS = [
    ('1-1 UART+DMA+空闲中断', 'UART_DMA_IDLE', ['rcc', 'gpio', 'usart', 'dma', 'spi']),
    ('2-1 IIC+DMA+中断', 'IIC_DMA_IRQ', ['rcc', 'gpio', 'usart', 'dma', 'i2c', 'spi']),
    ('3-1 CAN+中断', 'CAN_IRQ', ['rcc', 'gpio', 'usart', 'can', 'spi']),
    ('4-1 TIM输入捕获+编码器', 'TIM_CAPTURE_ENCODER', ['rcc', 'gpio', 'usart', 'tim', 'spi']),
    ('5-1 定时器PID前馈速度闭环', 'MOTOR_PID_FF', ['rcc', 'gpio', 'usart', 'dma', 'tim', 'spi']),
    ('6-1 SPI OLED显示', 'SPI_OLED', ['rcc', 'gpio', 'usart', 'spi']),
    ('7-1 FreeRTOS多任务+UART+OLED', 'FREERTOS_UART_OLED', ['rcc', 'gpio', 'usart', 'dma']),
    ('8-1 摇杆OLED飞机避障', 'JOYSTICK_PLANE_GAME', ['rcc', 'gpio', 'adc']),
    ('9-1 串口ROS双向遥控', 'SERIAL_ROS_REMOTE', ['rcc', 'gpio', 'adc', 'usart', 'dma']),
    ('10-1 摇杆OLED飞机向上避障', 'JOYSTICK_PLANE_UP_GAME', ['rcc', 'gpio', 'adc']),
    ('11-1 AUBO笛卡尔串口遥控', 'AUBO_CARTESIAN_REMOTE', ['rcc', 'gpio', 'adc', 'usart', 'dma']),
    ('12-1 ROS移动机器人下层控制器', 'ROS_MOBILE_CONTROLLER', ['rcc', 'gpio', 'usart', 'dma', 'tim', 'i2c']),
]

# Target页使用OCR_RVCT4/9；仅修改Cpu和IROM/IRAM不会同步这两组字段。
MEMORY_REGIONS = {
    'IROM': (1, 0x08000000, 0x10000),
    'OCR_RVCT4': (1, 0x08000000, 0x10000),
    'IRAM': (0, 0x20000000, 0x5000),
    'OCR_RVCT9': (0, 0x20000000, 0x5000),
}


def set_memory_layout(option):
    """统一本仓库F103C8的设备内存和Target对话框内存，保留其他工程选项。"""
    for name, values in MEMORY_REGIONS.items():
        for field, value in zip(('Type', 'StartAddress', 'Size'), values):
            node = option.find('TargetArmAds/ArmAdsMisc/OnChipMemories/' + name + '/' + field)
            if node is None:
                raise ValueError(f'Missing memory setting: {name}/{field}')
            node.text = str(value) if field == 'Type' else f'0x{value:08X}'

def add(parent, name, value=''):
    node = ET.SubElement(parent, name)
    node.text = str(value)
    return node

def generate(selected=None):
    template = ET.parse(ROOT / 'tools/TargetOption.xml').getroot()
    for folder, target_name, modules in PROJECTS:
        if selected is not None and folder not in selected:
            continue
        project_dir = ROOT / folder
        project_path = project_dir / 'Project.uvprojx'
        existing = project_path.exists()
        native_format = False
        if existing:
            native_format = b'standalone="no"' in project_path.read_bytes()
            root = ET.parse(project_path).getroot()
            target = root.find('./Targets/Target')
            assert target is not None
            option = target.find('TargetOption')
            assert option is not None
        else:
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
            if not existing:
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
            if not existing:
                set_option('TargetArmAds/' + tag, value)
        if (project_dir / 'FreeRTOS').is_dir() and not existing:
            set_option('TargetArmAds/Cads/VariousControls/IncludePath',
                       '.\\Start;.\\Library;.\\User;.\\System;.\\Hardware;'
                       '.\\FreeRTOS\\include;.\\FreeRTOS\\portable\\RVDS\\ARM_CM3')
        set_memory_layout(option)
        # Preserve other compiler/debug/download settings and per-file options.
        old_files = {f.findtext('FilePath'): deepcopy(f) for f in target.findall('./Groups/Group/Files/File')}
        previous_groups = target.find('Groups')
        if previous_groups is not None:
            target.remove(previous_groups)
        groups = add(target, 'Groups')
        group_names = ['Start', 'Library', 'User', 'System', 'Hardware']
        if (project_dir / 'FreeRTOS').is_dir():
            group_names.append('FreeRTOS')
        for group_name in group_names:
            group = add(groups, 'Group')
            add(group, 'GroupName', group_name)
            files = add(group, 'Files')
            paths = sorted((project_dir / group_name).glob('*'))
            if group_name == 'FreeRTOS':
                paths = sorted((project_dir / group_name).rglob('*'))
            if group_name == 'Start':
                paths = [p for p in paths if p.name != 'core_cm3.c']  # ARMCC使用编译器内建核心指令
            if group_name == 'Library':
                paths = [p for p in paths if p.stem in ['misc'] + ['stm32f10x_' + m for m in modules]]
            for path in paths:
                if path.suffix not in ('.c', '.h', '.s', '.sct'):
                    continue
                relative = '.\\' + str(path.relative_to(project_dir)).replace('/', '\\')
                if relative in old_files:
                    files.append(old_files[relative])
                    continue
                f = add(files, 'File')
                add(f, 'FileName', path.name)
                add(f, 'FileType', {'.c': 1, '.s': 2}.get(path.suffix, 5))
                add(f, 'FilePath', relative)
        ET.indent(root, space='  ')
        # uVision may rewrite project XML with CRLF; match it to avoid Git churn.
        data = ET.tostring(root, encoding='utf-8', xml_declaration=not native_format, short_empty_elements=not native_format)
        if native_format:
            data = b'<?xml version="1.0" encoding="UTF-8" standalone="no" ?>\n' + data
        (project_dir / 'Project.uvprojx').write_bytes(data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))

if __name__ == '__main__':
    generate()
