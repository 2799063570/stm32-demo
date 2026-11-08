"""STM32 robot fixed 48-byte protocol. No ROS or serial dependency."""
import binascii
import struct


def command(sequence, left=0, right=0, flags=0):
    if not (-200 <= left <= 200 and -200 <= right <= 200 and 0 <= flags <= 7):
        raise ValueError('command out of range')
    data = bytearray(48)
    struct.pack_into('<2sBBBBHhhB', data, 0, b'\xaa\x55', 1, 1, 48, 0,
                     sequence & 65535, left, right, flags)
    struct.pack_into('<H2s', data, 44, binascii.crc_hqx(data[:44], 0xffff), b'\r\n')
    return bytes(data)


class Parser:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while len(self.buffer) >= 48:
            f = self.buffer[:48]
            if (f[:5] == b'\xaa\x55\x01\x81\x30' and f[-2:] == b'\r\n'
                    and struct.unpack_from('<H', f, 44)[0] == binascii.crc_hqx(f[:44], 0xffff)):
                frames.append(f)
                del self.buffer[:48]
            else:
                del self.buffer[0]
        return frames
