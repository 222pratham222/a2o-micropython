**Running a Python REPL on an open-source POWER ISA core in Verilator RTL simulation.**## What is this?

MicroPython running bare-metal on the **A2O** core — an open-source 64-bit Power ISA processor designed by IBM and released through the OpenPOWER Foundation — simulated cycle-accurately in **Verilator**.

## Architecture

| Component | Implementation |
|-----------|---------------|
| CPU Core | A2O (a2owb Verilog), dual-issue, out-of-order, 64-bit Bi-Endian|
| Bus | Wishbone B4, 32-bit byte-addressed |
| Memory | C++ hash map (flat, zero-latency) |
| UART | CSR intercept at 0xFFF04000 (LiteX-compatible) |
| Simulator | Verilator (cycle-accurate) |

## Quick Start

### Prerequisites

- [Verilator](https://www.veripool.org/verilator/) 4.x+
- `powerpc64-linux-gnu-gcc` cross-compiler
- [A2O RTL](https://github.com/OpenPOWERFoundation/a2o.git)
- [A2O RTL](https://codeberg.org/PowerCommons/a2o.git)
- [MicroPython source](https://github.com/micropython/micropython)

### Build MicroPython

```bash
cd micropython/ports/powerpc
cp /path/to/a2o-micropython/src/head.S .
cp /path/to/a2o-micropython/src/powerpc.lds .
make UART=litex CROSS_COMPILE=powerpc64-buildroot-linux-gnu- -j$(nproc) clean
make UART=litex CROSS_COMPILE=powerpc64-buildroot-linux-gnu- -j$(nproc)

# convert from bin to hex copy to verilator folder

python3 ~ /a2o-micropython/tools/bin2hex.py \
    build/firmware.bin \
    ~/a2o/dev/sim/verilator/cmod7_kintex_rom.init 100000

```
### Copy micropython Testbench to verilator directory

```bash 
cp /path/to/a2o-micropython/src/tb_litex_mp.cpp .
```

### Compile in verilator using a2o wishbone wrapper
```bash
cd a2o/dev/sim/verilator

verilator -cc --exe --trace --Mdir obj_dir \
    --language 1364-2001 \
    -Wno-fatal -Wno-LITENDIAN --error-limit 1 \
    -Iverilog/a2o_litex -Iverilog/work \
    -Iverilog/trilib -Iverilog/unisims \
    a2owb.v tb_litex_mp.cpp |& tee verilator.txt


```
### Build and Run Simulation

```bash
cd a2o/dev/sim/verilator

cp /path/to/micropython/ports/powerpc/build/cmod7_kintex_rom.init .

make -C ~/a2o/dev/sim/verilator/obj_dir -f Va2owb.mk Va2owb

obj_dir/Va2owb
# Wait ~5 minutes for the MicroPython banner
```

## Key Challenges Solved

### ERAT Configuration (I-ERAT vs D-ERAT)

The A2O core uses separate Instruction and Data ERATs controlled by MMUCR0:
- `0x8C000000` → TLBSEL=11 → D-ERAT (data access)
- `0x88000000` → TLBSEL=10 → I-ERAT (instruction fetch)

| ERAT | Entry | VA Base | Size | Purpose |
|------|-------|---------|------|---------|
| D-ERAT | 31 | 0x00000000 | 1 MB | Firmware code+data |
| D-ERAT | 30 | 0x00100000 | 1 MB | Headroom |
| D-ERAT | 29 | 0x03000000 | 1 MB | Stack |
| D-ERAT | 28 | 0xFFF00000 | 64 KB | UART I/O (cache-inhibited) |
| I-ERAT | 15 | 0x00000000 | 1 MB | Firmware fetch |
| I-ERAT | 13 | 0x00100000 | 1 MB | Headroom |

### Big-Endian UART Byte Ordering

On big-endian PowerPC, UART TX data lands in the upper byte: `(data >> 24) & 0xFF`. RX must place the character at `char << 24`.

### Wishbone Bus Timing

Address and data must be captured on the request cycle (when `cyc && stb` assert), not on the ack cycle one clock later.

### A2O Exception Vectors

A2O uses 0x20-spaced vectors (Book III-E): DSI at 0x060, ISI at 0x080, DTLB miss at 0x1C0 — not the Book-S 0x100 spacing.

## Files

| File | Description |
|------|-------------|
| `src/head.S` | Boot code: exception vectors, ERAT setup, BSS clear, call main |
| `src/powerpc.lds` | Linker script for flat binary layout from 0x0 |
| `src/tb_litex_mp.cpp` | Verilator testbench: Wishbone memory + UART intercept |
| `docs/micropython_a2o_report.pdf` | Detailed technical report |

## Performance

RTL simulation is slow. The banner takes ~5 minutes, each keystroke takes minutes to process. On FPGA it would be instant.

## References

- [A2O Core](https://codeberg.org/PowerCommons/a2o.git)
- [MicroPython](https://micropython.org/)
- [OpenPOWER Foundation](https://openpowerfoundation.org/)
- [Verilator](https://www.veripool.org/verilator/)
- [PowerCommons](https://powercommons.org/)
## License

MIT License for testbench and boot code. A2O core is CC-BY 4.0 (IBM/OpenPOWER). MicroPython is MIT.
