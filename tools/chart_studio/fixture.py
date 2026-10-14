import struct

def vlq(value):
    data = [value & 127]
    while value >> 7:
        value >>= 7
        data.insert(0, 128 | (value & 127))
    return bytes(data)

def fixture(path):
    events = [(0, b'\xff\x51\x03\x07\xa1\x20'), (0, b'\xc0\x1e')]
    for repeat in range(3):
        for index, pitch in enumerate([64, 66, 67, 71, 69]):
            tick = repeat * 3840 + index * 240
            events += [(tick, bytes([0x90, pitch, 100])), (tick + 120, bytes([0x80, pitch, 0]))]
    data = bytearray()
    previous = 0
    for tick, event in sorted(events, key=lambda e: e[0]):
        data += vlq(tick - previous) + event
        previous = tick
    data += b'\0\xff\x2f\0'
    path.write_bytes(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(data)) + data)

