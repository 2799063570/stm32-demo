"""离线生成串口助手HEX报文；复用协议v2，不打开串口、不需要ROS或pyserial。"""
import argparse
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ros1/aubo_remote_bridge/src'))
from aubo_remote_bridge.protocol import Frame, POLL, SOURCE_DEMO, feedback_frame

SESSION = 0x10203040
SEQUENCE = 42
JOINTS = (0, 1571, -1571, 3142, -3142, 500)
COMMANDS = {'none': 0, 'home': 1, 'zero': 2, 'cancel': 3}
STATES = {'idle': 0, 'accept': 1, 'plan': 2, 'run': 3, 'done': 4,
          'failed': 5, 'canceling': 6, 'canceled': 7, 'reject': 8,
          'verify': 9, 'unknown': 10}


def poll_bytes():
    return Frame(POLL, SEQUENCE, struct.pack('<I', SESSION)).encode()


def demo_packet(ack_id=0, command='none', state='idle'):
    """两帧紧邻发送，让反馈在最近POLL的100ms期限内到达。

    固定会话/序号仅用于手动台架调试；正常ROS桥继续使用自己的递增序号。
    来源始终标为DEMO，关节角和动作应答均为手动模拟值。
    """
    code, status = COMMANDS[command], STATES[state]
    target = code if code in (1, 2) else 0
    feedback = feedback_frame(SESSION, SEQUENCE, SOURCE_DEMO, 63, JOINTS,
                              ack_id, code, status, status, target)
    return poll_bytes() + feedback.encode()


def hex_line(data):
    return data.hex(' ').upper()


def write_examples():
    folder = ROOT / '9-1 串口ROS双向遥控/串口助手报文'
    folder.mkdir(exist_ok=True)
    examples = {
        '01_读取摇杆': poll_bytes(),
        '02_模拟六关节反馈': demo_packet(),
        '03_HOME请求1完成': demo_packet(1, 'home', 'done'),
    }
    for name, data in examples.items():
        # 文本供HEX发送框复制；bin供“发送文件/原始二进制”功能使用。
        (folder / (name + '.hex.txt')).write_text(hex_line(data) + '\n', encoding='ascii')
        (folder / (name + '.bin')).write_bytes(data)
    print(folder)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--poll-only', action='store_true', help='只生成读取摇杆的POLL')
    parser.add_argument('--ack-id', type=int, default=0, help='要应答的请求编号，0～65535')
    parser.add_argument('--command', choices=COMMANDS, default='none')
    parser.add_argument('--state', choices=STATES, default='idle')
    parser.add_argument('--write-examples', action='store_true', help='更新第9项内的示例文件')
    args = parser.parse_args()
    if not 0 <= args.ack_id <= 65535:
        parser.error('--ack-id必须为0～65535')
    if (args.ack_id == 0) != (args.command == 'none'):
        parser.error('没有命令时编号为0；应答HOME/ZERO/CANCEL时编号须为1～65535')
    if args.write_examples:
        write_examples()
    else:
        print(hex_line(poll_bytes() if args.poll_only else demo_packet(args.ack_id, args.command, args.state)))


if __name__ == '__main__':
    main()
