#!/usr/bin/env python3
"""
bin2hex.py - Convert big-endian binary to $readmemh hex.
No byte swapping needed — firmware and sim are both big-endian.
Usage: python3 bin2hex.py input.bin output.init [max_words]
"""
import sys
import struct

def bin2hex(input_path, output_path, max_words=100000):
    with open(input_path, 'rb') as f:
        data = f.read()

    remainder = len(data) % 4
    if remainder:
        data += b'\x00' * (4 - remainder)

    nwords = len(data) // 4
    if nwords > max_words:
        print(f"Truncating {nwords} -> {max_words} words")
        nwords = max_words
        data = data[:nwords * 4]

    with open(output_path, 'w') as f:
        for i in range(nwords):
            b = data[i*4 : i*4+4]
            # Big-endian — no swap needed
            word = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]
            f.write(f'{word:08x}\n')

    print(f"Wrote {nwords} words to {output_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    bin2hex(sys.argv[1], sys.argv[2],
            int(sys.argv[3]) if len(sys.argv) > 3 else 100000)
