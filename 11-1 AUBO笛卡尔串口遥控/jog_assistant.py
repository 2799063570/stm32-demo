"""第11项串口助手：离线报文/INPUT解码与台架模拟。只连接STM32，不调用ROS或机械臂。"""
import argparse
from dataclasses import dataclass
from pathlib import Path
import secrets
import struct
import time

POLL, INPUT, FEEDBACK = 0x11, 0x12, 0x13
SESSION = 0x12345678
JOINTS = (0, 1571, -1571, 3142, -3142, 500)
COMMANDS = {'none': 0, 'enable': 1, 'stop': 2, 'home': 3, 'cancel': 4, 'reset': 5}
ACKS = {'idle': 0, 'accept': 1, 'run': 3, 'done': 4, 'failed': 5, 'stopping': 6, 'canceled': 7, 'reject': 8}
CONTROLLERS = {'unknown': 0, 'waiting': 1, 'disabled': 2, 'enabled': 3, 'jogging': 4,
               'stopping': 5, 'homing': 6, 'home-stopping': 7, 'fault': 8}


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


@dataclass(frozen=True)
class Frame:
    kind: int
    sequence: int
    payload: bytes

    def encode(self):
        if not 0 <= self.sequence <= 65535 or len(self.payload) > 32:
            raise ValueError('sequence/length out of range')
        body = struct.pack('<BBHB', 2, self.kind, self.sequence, len(self.payload)) + self.payload
        return b'\xa5\x5a' + body + struct.pack('<H', crc16(body))


class Parser:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while len(self.buffer) >= 2:
            if self.buffer[:2] != b'\xa5\x5a':
                del self.buffer[0]
                continue
            if len(self.buffer) < 7:
                break
            if self.buffer[2] != 2 or self.buffer[6] > 32:
                del self.buffer[0]
                continue
            total = self.buffer[6] + 9
            if len(self.buffer) < total:
                break
            wire = self.buffer[:total]
            if crc16(wire[2:-2]) != struct.unpack('<H', wire[-2:])[0]:
                del self.buffer[0]
                continue
            result.append(Frame(wire[3], struct.unpack('<H', wire[4:6])[0], bytes(wire[7:-2])))
            del self.buffer[:total]
        return result


def poll_frame(session, sequence):
    return Frame(POLL, sequence, struct.pack('<I', session))


def decode_input(frame):
    if frame.kind != INPUT or len(frame.payload) != 18:
        raise ValueError('expected 0x12 INPUT with 18-byte payload')
    session, request, command, flags, coordinate, reserved, speed, x, y, z = struct.unpack('<IHBBBBHhhh', frame.payload)
    if reserved or command > 5 or flags > 7 or coordinate > 1 or speed > 1000 or ((request == 0) != (command == 0)):
        raise ValueError('invalid INPUT fields')
    if any(abs(axis) > 1000 for axis in (x, y, z)) or (speed == 0 and (x or y or z)):
        raise ValueError('invalid direction/speed')
    return dict(session=session, request_id=request, command=command, flags=flags,
                frame=coordinate, speed=speed, axes=(x, y, z))


def feedback_frame(session, sequence, request=0, command=0, ack=0, controller=2,
                   flags=5, mask=63, joints=JOINTS, home_state=255, home_error=65535, fault=0):
    if not 0 <= request <= 65535 or not 0 <= command <= 5 or ((request == 0) != (command == 0)):
        raise ValueError('command/request_id must be paired')
    if ack not in ACKS.values() or not 0 <= controller <= 8 or not 0 <= flags <= 15 or not 0 <= mask <= 63:
        raise ValueError('invalid feedback state/flags/mask')
    if home_state not in (*range(10), 255) or not 0 <= home_error <= 65535 or not 0 <= fault <= 65535:
        raise ValueError('invalid home/fault fields')
    payload = struct.pack('<IHBBBBBB6hHH', session, request, command, ack, controller, home_state,
                          flags, mask, *joints, home_error, fault)
    return Frame(FEEDBACK, sequence, payload)


def examples():
    return {
        '01_poll_seq1': poll_frame(SESSION, 1).encode(),
        '02_joints_seq2': poll_frame(SESSION, 2).encode() + feedback_frame(SESSION, 2).encode(),
        '03_joints_invalid_seq3': poll_frame(SESSION, 3).encode() + feedback_frame(SESSION, 3, mask=0).encode(),
        '04_fault_seq4': poll_frame(SESSION, 4).encode() + feedback_frame(SESSION, 4, controller=8, fault=5).encode(),
    }


def write_examples():
    folder = Path(__file__).resolve().parent / '串口助手报文'
    folder.mkdir(exist_ok=True)
    for name, wire in examples().items():
        (folder / (name + '.bin')).write_bytes(wire)
        (folder / (name + '.hex.txt')).write_text(wire.hex(' ').upper() + '\n', encoding='ascii')
    print(folder)


class Simulator:
    """只生成模拟反馈；角度为固定样本，HOME延时也是模拟值，不驱动机器人。"""
    def __init__(self):
        self.baseline = None
        self.last_command = self.request = self.command = self.ack = 0
        self.enabled = False
        self.home_until = None
        self.home_state, self.home_error = 255, 65535
        self.joints = JOINTS

    def reply(self, state, sequence, now):
        valid = state['flags'] & 3 == 3
        neutral = not any(state['axes']) and not (state['flags'] & 4)
        request, command = state['request_id'], state['command']
        if self.home_until is not None and now >= self.home_until:
            self.home_until = None
            self.home_state, self.home_error, self.ack = 3, 0, 4
            self.joints = (0, 0, 1629, 0, 1663, 0)
        if not valid:
            self.enabled = False
            if self.home_until is not None:
                self.home_until = None
                self.home_state, self.ack = 2, 7
        if self.baseline is None:
            self.baseline, self.last_command = request, command  # 首份INPUT只建立旧请求基线
        elif request and (request != self.baseline or command != self.last_command):
            forward = (request - self.baseline) & 65535
            self.request, self.command = request, command
            if not 1 <= forward <= 32767:
                self.ack, self.enabled = 8, False
                self.home_until = None
            else:
                self.baseline, self.last_command = request, command
                busy = self.home_until is not None
                if command == 2 or command == 4:
                    if busy:
                        self.home_state = 2
                    self.enabled, self.home_until = False, None
                    self.ack = 4 if command == 2 else 7
                elif not valid or not neutral or busy or self.enabled:
                    self.ack = 8
                elif command == 1:
                    self.enabled, self.ack = True, 4
                elif command == 3:
                    self.home_until = now + 2.0
                    self.home_state, self.home_error, self.ack = 1, 1000, 3
                elif command == 5:
                    self.ack = 4
        controller = 6 if self.home_until is not None else 2
        if self.enabled:
            controller = 4 if state['flags'] & 4 and any(state['axes']) else 3
        flags = (1 if valid else 0) | (2 if self.enabled else 0) | 4
        return feedback_frame(state['session'], sequence, self.request, self.command, self.ack, controller,
                              flags, joints=self.joints, home_state=self.home_state, home_error=self.home_error)


def send(port, wire):
    if port.write(wire) != len(wire):
        raise OSError('串口发送不完整')


def poll_once(port, parser, simulator, session, sequence, timeout=0.1, clock=time.monotonic):
    send(port, poll_frame(session, sequence).encode())
    deadline = clock() + timeout
    while clock() < deadline:
        for frame in parser.feed(port.read(256)):
            if clock() >= deadline or frame.kind != INPUT or frame.sequence != sequence:
                continue
            try:
                state = decode_input(frame)
            except ValueError:
                continue
            if state['session'] != session:
                continue
            feedback = simulator.reply(state, sequence, clock())
            send(port, feedback.encode())
            return state, feedback
    return None


def run(port_name):
    try:
        import serial  # 仅此子命令需要pyserial；离线生成/解码不需要安装额外包
    except ImportError:
        raise SystemExit('缺少pyserial；请先运行 python -m pip install pyserial') from None
    print('SIMULATION / 仅STM32台架调试：所有关节、ENABLED和DONE反馈均为模拟，不连接ROS或机械臂。')
    print('请先关闭占用此COM口的串口助手。Ctrl+C退出；随后STM32按100ms反馈超时锁定。')
    try:
        with serial.Serial(port_name, 115200, timeout=0.005, write_timeout=0.1) as port:
            port.reset_input_buffer()
            simulator, parser = Simulator(), Parser()
            session, sequence, previous = secrets.randbits(32) or 1, 0, None
            while True:
                start = time.monotonic()
                if sequence == 65535:
                    session, sequence, simulator, parser = (session + 1) & 0xFFFFFFFF, 0, Simulator(), Parser()
                sequence += 1
                result = poll_once(port, parser, simulator, session, sequence)
                if result is None:
                    if previous != 'timeout':
                        print('[SIM] INPUT超时，清除模拟使能；等待连接，新会话不执行缓存请求。')
                    previous = 'timeout'
                    session, sequence, simulator, parser = (session + 1) & 0xFFFFFFFF, 0, Simulator(), Parser()
                else:
                    state, frame = result
                    summary = (state['request_id'], state['command'], state['flags'], state['frame'], state['speed'], state['axes'], frame.payload[7:11])
                    if summary != previous:
                        print(f"[SIM] seq={sequence} req={state['request_id']} cmd={state['command']} "
                              f"flags={state['flags']:02X} frame={state['frame']} speed={state['speed'] / 10:g}mm/s "
                              f"XYZ={state['axes']} -> ack={frame.payload[7]} ctrl={frame.payload[8]}")
                        previous = summary
                time.sleep(max(0, 0.02 - (time.monotonic() - start)))
    except KeyboardInterrupt:
        print('\n模拟工具已退出，串口已关闭。')
    except (serial.SerialException, OSError) as error:
        raise SystemExit(f'串口错误：{error}') from None


def integer(text):
    return int(text, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='mode', required=True)
    commands.add_parser('examples', help='生成固定序号HEX和BIN示例；不可循环同一序号')
    frame = commands.add_parser('frame', help='离线生成一份新序号POLL或POLL+FEEDBACK')
    frame.add_argument('--session', type=integer, default=SESSION)
    frame.add_argument('--sequence', type=integer, default=1)
    frame.add_argument('--feedback', action='store_true')
    frame.add_argument('--request-id', type=integer, default=0)
    frame.add_argument('--command', choices=COMMANDS, default='none')
    frame.add_argument('--ack', choices=ACKS, default='idle')
    frame.add_argument('--controller', choices=CONTROLLERS, default='disabled')
    frame.add_argument('--flags', type=integer, default=5)
    frame.add_argument('--joint-mask', type=integer, default=63)
    frame.add_argument('--fault', type=integer, default=0)
    decode = commands.add_parser('decode', help='解码从串口助手复制的完整INPUT十六进制数据')
    decode.add_argument('hex', help='带引号的十六进制数据，支持多帧和空格/换行')
    serial_run = commands.add_parser('run', help='自动20ms轮询STM32并提供模拟控制器反馈')
    serial_run.add_argument('--port', required=True, help='例如COM5或/dev/ttyUSB0')
    args = parser.parse_args()
    try:
        if args.mode == 'examples':
            write_examples()
        elif args.mode == 'frame':
            if not 0 <= args.session <= 0xFFFFFFFF:
                raise ValueError('session must be 0..0xFFFFFFFF')
            wire = poll_frame(args.session, args.sequence).encode()
            if args.feedback:
                wire += feedback_frame(args.session, args.sequence, args.request_id, COMMANDS[args.command], ACKS[args.ack],
                                       CONTROLLERS[args.controller], args.flags, args.joint_mask, fault=args.fault).encode()
            print(wire.hex(' ').upper())
        elif args.mode == 'decode':
            frames = Parser().feed(bytes.fromhex(args.hex))
            if not frames:
                raise ValueError('没有完整CRC正确的帧，请去除时间戳并复制完整INPUT')
            for frame in frames:
                state = decode_input(frame)
                print(f'seq={frame.sequence} {state}')
        else:
            run(args.port)
    except (ValueError, struct.error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
