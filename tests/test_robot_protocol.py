from pathlib import Path
import sys
import binascii
import struct
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'ros1/mobile_robot_bridge/scripts'))
from robot_protocol import command, Parser

assert command(65535, 100, -100, 1) == (ROOT / 'tests/build/robot_golden.bin').read_bytes()
assert command(65536)[6:8] == b'\0\0'
try:
    command(0, 201)
    raise AssertionError('range accepted')
except ValueError:
    pass
f = bytearray(command(4)); f[3] = 0x81
struct.pack_into('<H', f, 44, binascii.crc_hqx(f[:44], 0xffff))
for offset in range(48):
    p = Parser()
    damaged = bytearray(f); damaged[offset] ^= 0x5a
    assert p.feed(b'noise' + damaged + f + f) == [f, f]
p = Parser()
assert not p.feed(f[:20])
assert p.feed(f[20:]) == [f]
print('robot Python: C golden frame interoperability and feedback resynchronization passed')
