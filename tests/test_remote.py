"""Offline tests for the real PC protocol/core/transport; no ROS or robot required."""
import math
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ros1/aubo_remote_bridge/src'))
from aubo_remote_bridge.protocol import (Frame, Parser, POLL, INPUT, FEEDBACK, INPUT_STRUCT,
    FEEDBACK_STRUCT, JOINT_NAMES, crc16, decode_input, feedback_frame)
from aubo_remote_bridge.core import BridgeCore, JointFeedback
from aubo_remote_bridge.transport import SerialTransport


def input_frame(session=0x10203040, sequence=42, x=1000, y=1000, flags=3):
    return Frame(INPUT, sequence, INPUT_STRUCT.pack(session, 4095, 0, x, y, 0, flags, 0, 0, 0))


class ProtocolTests(unittest.TestCase):
    def test_c_python_wire_agreement(self):
        self.assertEqual(crc16(b'123456789'), 0x29b1)
        cases = {
            'remote_poll_frame.bin': Frame(POLL, 42, struct.pack('<I', 0x10203040)),
            'remote_input_frame.bin': input_frame(),
            'remote_feedback_frame.bin': feedback_frame(0x10203040, 42, 1, 63, (0, 1571, -1571, 3142, -3142, 500)),
        }
        for name, frame in cases.items():
            with self.subTest(name=name):
                actual = (ROOT / 'tests/build' / name).read_bytes()
                self.assertEqual(actual, frame.encode())
                self.assertEqual(Parser().feed(actual), [frame])

    def test_stream_recovery(self):
        frame = input_frame()
        data = frame.encode()
        for cut in range(len(data) + 1):
            parser = Parser()
            self.assertEqual(parser.feed(data[:cut]) + parser.feed(data[cut:]), [frame])
        bad = bytearray(data)
        bad[10] ^= 1
        parser = Parser()
        self.assertEqual(parser.feed(b'noise' + bad + data + data), [frame, frame])
        self.assertGreater(parser.errors, 0)
        for header in (b'\xa5\x5a\x03\x01\x00\x00\x00', b'\xa5\x5a\x02\x01\x00\x00\xff'):
            self.assertEqual(Parser().feed(header + data), [frame])
        for _ in range(1000):
            parser.feed(b'x' * 128)
            self.assertLessEqual(len(parser.buffer), 41)
        self.assertEqual(parser.feed(data), [frame])

    def test_semantics(self):
        state = decode_input(input_frame(x=-1000, y=0), 0x10203040)
        self.assertEqual(state.axes, (-1.0, 0.0))
        self.assertEqual(decode_input(input_frame(x=0, y=0, flags=2), 0x10203040).axes, (0.0, 0.0))
        for frame in (input_frame(x=1001), input_frame(flags=0), input_frame(session=9)):
            with self.assertRaises(ValueError):
                decode_input(frame, 0x10203040)


class CoreTests(unittest.TestCase):
    def test_polling_and_expiry(self):
        core = BridgeCore(session=0x10203040)
        core.sequence = 41
        self.assertEqual(core.request(0).sequence, 42)
        self.assertIsNone(core.request(0.01))
        self.assertIsNotNone(core.receive(input_frame(), 0.01))
        self.assertTrue(core.online(0.02))
        self.assertIsNone(core.receive(input_frame(), 0.015))  # duplicate response
        request = core.request(0.02)
        self.assertEqual(request.sequence, 43)
        self.assertIsNone(core.receive(input_frame(sequence=42), 0.03))
        self.assertIsNone(core.receive(input_frame(sequence=43, session=1), 0.03))
        self.assertFalse(core.online(0.12))
        self.assertIsNone(core.receive(input_frame(sequence=43), 0.13))  # late but newly received
        self.assertEqual(core.request(0.13).sequence, 44)
        self.assertIsNone(core.last_input)

    def test_wrap_and_calibration(self):
        core = BridgeCore(session=1)
        core.sequence = 65535
        self.assertEqual(core.request(0).sequence, 1)
        self.assertEqual(core.session, 2)
        self.assertIsNone(core.receive(input_frame(session=1, sequence=1), 0.01))
        state = core.receive(input_frame(session=2, sequence=1, flags=2, x=0, y=0), 0.02)
        self.assertFalse(state.valid)
        self.assertTrue(core.online(0.02))  # connected and calibrated are distinct

    def test_joint_feedback_freshness(self):
        feedback = JointFeedback()
        self.assertEqual(feedback.snapshot(0)[1], 0)
        feedback.update(list(reversed(JOINT_NAMES)), [0, 1, 2, 3, 4, 5], 1)
        self.assertEqual(feedback.snapshot(1.1), (1, 63, (5000, 4000, 3000, 2000, 1000, 0)))
        self.assertEqual(feedback.snapshot(1.51), (0, 0, (0,) * 6))
        feedback.update([JOINT_NAMES[0]], [math.nan], 2)
        self.assertEqual(feedback.snapshot(2)[1], 0)
        feedback.update([JOINT_NAMES[0]], [1e308], 2)
        self.assertEqual(feedback.snapshot(2)[1], 0)
        feedback.update([JOINT_NAMES[0]], [-0.123], 3)
        self.assertEqual(feedback.snapshot(3), (1, 1, (-123, 0, 0, 0, 0, 0)))
        feedback.update([JOINT_NAMES[0], JOINT_NAMES[0]], [1, 2], 4)
        self.assertEqual(feedback.snapshot(4)[1], 0)


class FakeSerial:
    """Explicit serial byte-stream double; not an electrical UART/USB simulation."""
    def __init__(self, **kwargs):
        self.kwargs = kwargs
        self.rx = bytearray()
        self.written = []
        self.closed = False
        self.partial = False
    def reset_input_buffer(self): self.rx.clear()
    def reset_output_buffer(self): pass
    @property
    def in_waiting(self): return len(self.rx)
    def read(self, count):
        data = bytes(self.rx[:count]); del self.rx[:count]; return data
    def write(self, data):
        self.written.append(data)
        return len(data) - 1 if self.partial else len(data)
    def close(self): self.closed = True


class TransportTests(unittest.TestCase):
    def test_poll_input_feedback_round_trip(self):
        transport = SerialTransport('TEST', factory=FakeSerial)
        core = BridgeCore(session=0x10203040)
        core.sequence = 41
        transport.write(core.request(0))
        self.assertEqual(transport.port.written[-1], (ROOT / 'tests/build/remote_poll_frame.bin').read_bytes())
        data = (ROOT / 'tests/build/remote_input_frame.bin').read_bytes()
        for byte in data:
            transport.port.rx.append(byte)
            for frame in transport.read():
                state = core.receive(frame, 0.01)
                self.assertTrue(state.valid)
                transport.write(feedback_frame(core.session, frame.sequence, 1, 63, (0, 1571, -1571, 3142, -3142, 500)))
        self.assertEqual(transport.port.written[-1], (ROOT / 'tests/build/remote_feedback_frame.bin').read_bytes())
        self.assertLessEqual(transport.port.kwargs['timeout'], 0.01)
        transport.port.partial = True
        with self.assertRaises(IOError): transport.write(Frame(POLL, 1, b'1234'))
        transport.close()
        self.assertTrue(transport.port.closed)



if __name__ == '__main__':
    unittest.main()
