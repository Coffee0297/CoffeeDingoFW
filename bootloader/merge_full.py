#!/usr/bin/env python3
"""Merge an OpenBLT bootloader and an application (both Intel HEX) into one image for a blank board.

    python3 bootloader/merge_full.py <bootloader.hex> <app.hex> <out-base>

Writes <out-base>.hex and <out-base>.bin (raw from 0x08000000, gaps 0xFF) for SWD or USB-DFU. Refuses
a bootloader that does not start at 0x08000000 and images that overlap. Standard library only.
"""
import sys

FLASH = 0x08000000


def read_hex(path):
    mem, base = {}, 0
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line.startswith(':'):
            continue
        raw = bytes.fromhex(line[1:])
        if sum(raw) & 0xFF:
            sys.exit(f'{path}:{n}: bad checksum')
        count, addr, kind, data = raw[0], int.from_bytes(raw[1:3], 'big'), raw[3], raw[4:4 + raw[0]]
        if kind == 0x00:
            for i, b in enumerate(data):
                mem[base + addr + i] = b
        elif kind == 0x02:
            base = int.from_bytes(data, 'big') << 4
        elif kind == 0x04:
            base = int.from_bytes(data, 'big') << 16
        elif kind == 0x01:
            break
    if not mem:
        sys.exit(f'{path}: no data')
    return mem


def record(kind, addr, data):
    raw = bytes([len(data), addr >> 8 & 0xFF, addr & 0xFF, kind]) + data
    return ':' + (raw + bytes([-sum(raw) & 0xFF])).hex().upper()


def write_hex(path, mem, entry):
    out, upper = [], None
    addrs = sorted(mem)
    i = 0
    while i < len(addrs):
        start = addrs[i]
        chunk = bytearray([mem[start]])
        # up to 16 contiguous bytes, not across a 64 KB boundary
        while i + 1 < len(addrs) and addrs[i + 1] == start + len(chunk) and len(chunk) < 16 and (start + len(chunk)) & 0xFFFF:
            i += 1
            chunk.append(mem[addrs[i]])
        if start >> 16 != upper:
            upper = start >> 16
            out.append(record(0x04, 0, upper.to_bytes(2, 'big')))
        out.append(record(0x00, start & 0xFFFF, bytes(chunk)))
        i += 1
    out.append(record(0x05, 0, entry.to_bytes(4, 'big')))
    out.append(':00000001FF')
    open(path, 'w', newline='\n').write('\n'.join(out) + '\n')


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    blt, app = read_hex(sys.argv[1]), read_hex(sys.argv[2])
    if min(blt) != FLASH:
        sys.exit(f'bootloader starts at 0x{min(blt):08X}, not 0x{FLASH:08X}')
    if max(blt) >= min(app):
        sys.exit(f'overlap: bootloader ends 0x{max(blt):08X}, app starts 0x{min(app):08X}')
    mem = {**blt, **app}
    # the board starts in the bootloader: its reset vector is the entry point
    entry = int.from_bytes(bytes(mem[FLASH + 4 + i] for i in range(4)), 'little')
    write_hex(sys.argv[3] + '.hex', mem, entry)
    end = max(mem) + 1
    open(sys.argv[3] + '.bin', 'wb').write(bytes(mem.get(a, 0xFF) for a in range(FLASH, end)))
    assert read_hex(sys.argv[3] + '.hex') == mem, 'merged .hex does not read back the same'
    print(f'{sys.argv[3]}: bootloader 0x{FLASH:08X}..0x{max(blt):08X} + app 0x{min(app):08X}..0x{max(app):08X}, '
          f'{(end - FLASH) // 1024} KB')


if __name__ == '__main__':
    main()
