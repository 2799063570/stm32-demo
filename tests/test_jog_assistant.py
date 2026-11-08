"""真实C固件报文与交付串口助手文件对照；串口链路测试使用显式FakePort。"""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '11-1 AUBO笛卡尔串口遥控'))
import jog_assistant as jog


def input_frame(sequence=1, session=jog.SESSION, request=0, command=0, flags=3, axes=(0, 0, 0)):
    return jog.Frame(jog.INPUT, sequence, struct.pack('<IHBBBBHhhh', session, request, command, flags, 0, 0, 100, *axes))


class AssistantTests(unittest.TestCase):
    def test_delivery_and_c_wire(self):
        for name, wire in jog.examples().items():
            folder = ROOT / '11-1 AUBO笛卡尔串口遥控/串口助手报文'
            self.assertEqual((folder / (name + '.bin')).read_bytes(), wire)
            self.assertEqual(bytes.fromhex((folder / (name + '.hex.txt')).read_text()), wire)
        self.assertEqual(jog.examples()['01_poll_seq1'], bytes.fromhex('A5 5A 02 11 01 00 04 78 56 34 12 EF 45'))
        self.assertEqual(jog.crc16(b'123456789'), 0x29B1)
        self.assertEqual((ROOT / 'tests/build/jog/jog_assistant_input.bin').read_bytes(), input_frame().encode())

    def test_stream_and_decode(self):
        wire = input_frame(axes=(-1000, 500, 0)).encode()
        bad = bytearray(wire)
        bad[12] ^= 1
        for cut in range(len(wire) + 1):
            parser = jog.Parser()
            frames = parser.feed(b'noise' + bad + wire[:cut]) + parser.feed(wire[cut:] + wire)
            self.assertEqual(len(frames), 2)
            self.assertEqual(jog.decode_input(frames[0])['axes'], (-1000, 500, 0))
        for frame in (input_frame(flags=8), input_frame(request=1), input_frame(axes=(1001, 0, 0))):
            with self.assertRaises(ValueError):
                jog.decode_input(frame)

    def test_simulated_lifecycle(self):
        sim = jog.Simulator()
        def reply(now=0, **fields):
            frame = input_frame(**fields)
            return sim.reply(jog.decode_input(frame), frame.sequence, now).payload
        reply()
        enabled = reply(request=1, command=1)
        self.assertEqual((enabled[7], enabled[8], enabled[10]), (4, 3, 7))
        moving = reply(request=1, command=1, flags=7, axes=(1000, 500, 0))
        self.assertEqual(moving[8], 4)
        self.assertEqual(reply(request=1, command=1)[8], 3)
        self.assertEqual(reply(request=2, command=2)[8], 2)
        home = reply(1.0, request=3, command=3)
        self.assertEqual((home[7], home[8], home[9]), (3, 6, 1))
        home_done = reply(3.1, request=3, command=3)
        self.assertEqual((home_done[7], home_done[8], home_done[9]), (4, 2, 3))
        self.assertEqual(struct.unpack('<6h', home_done[12:24]), (0, 0, 1629, 0, 1663, 0))
        reply(4.0, request=4, command=3)
        canceled = reply(4.5, request=5, command=4)
        self.assertEqual((canceled[7], canceled[8], canceled[9]), (7, 2, 2))
        reply(5.0, request=6, command=1)
        self.assertEqual(reply(5.1, request=6, command=1, flags=2)[8], 2)

    def test_baseline_and_dedup(self):
        sim = jog.Simulator()
        state = jog.decode_input(input_frame(request=19, command=3))
        sim.reply(state, 1, 0)
        self.assertIsNone(sim.home_until)
        sim.reply(state, 2, 0.1)
        self.assertIsNone(sim.home_until)
        state.update(request_id=20)
        sim.reply(state, 3, 1)
        self.assertEqual(sim.home_until, 3)
        sim.reply(state, 4, 2)
        self.assertEqual(sim.home_until, 3)
        state.update(command=1)
        self.assertEqual(sim.reply(state, 5, 2.1).payload[7], 8)
        self.assertFalse(sim.enabled)

    def test_port_orders_and_matches(self):
        class FakePort:
            def __init__(self):
                self.sent, self.incoming = [], bytearray()
            def write(self, wire):
                self.sent.extend(jog.Parser().feed(wire))
                if self.sent[-1].kind == jog.POLL:
                    self.incoming.extend(b'noise' + input_frame(7, 999).encode() + input_frame(6).encode() + input_frame(7).encode())
                return len(wire)
            def read(self, size):
                chunk = self.incoming[:min(size, 5)]
                del self.incoming[:len(chunk)]
                return chunk
        port = FakePort()
        result = jog.poll_once(port, jog.Parser(), jog.Simulator(), jog.SESSION, 7)
        self.assertIsNotNone(result)
        self.assertEqual([frame.kind for frame in port.sent], [jog.POLL, jog.FEEDBACK])
        self.assertEqual(port.sent[-1].sequence, 7)
        self.assertEqual(struct.unpack('<I', port.sent[-1].payload[:4])[0], jog.SESSION)

    def test_timeout_never_fabricates_feedback(self):
        class EmptyPort:
            def __init__(self):
                self.writes = []
            def write(self, wire):
                self.writes.append(wire)
                return len(wire)
            def read(self, size):
                return b''
        class Clock:
            value = 0
            def __call__(self):
                self.value += 0.025
                return self.value
        port = EmptyPort()
        self.assertIsNone(jog.poll_once(port, jog.Parser(), jog.Simulator(), jog.SESSION, 1, clock=Clock()))
        self.assertEqual(len(port.writes), 1)
        self.assertEqual(jog.Parser().feed(port.writes[0])[0].kind, jog.POLL)
        port.write = lambda wire: len(wire) - 1
        with self.assertRaises(OSError):
            jog.poll_once(port, jog.Parser(), jog.Simulator(), jog.SESSION, 2)


if __name__ == '__main__':
    unittest.main()
