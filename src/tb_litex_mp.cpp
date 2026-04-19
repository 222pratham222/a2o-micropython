// tb_litex_mp.cpp — MicroPython on A2O via tb_litex with UART CSR intercept

//#define TRACING

#include <cstddef>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <queue>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "verilated.h"
#include "Va2owb.h"
#include "Va2owb___024root.h"
#include "Va2owb_a2owb.h"
#include "Va2owb_a2l2wb.h"
#include "Va2owb_c.h"
#include "Va2owb_iuq.h"
#include "Va2owb_iuq_cpl_top.h"
#include "Va2owb_iuq_cpl.h"

#ifdef TRACING
#include "verilated_vcd_c.h"
VerilatedVcdC *t;
#else
unsigned int t = 0;
#endif

Va2owb* m;
Va2owb___024root* root;

vluint64_t main_time = 0;
double sc_time_stamp() { return main_time; }

const char*        tbName        = "tb_litex_mp";
const int          resetCycle    = 10;
const int          runCycles     = 2000000000;
const int          hbCycles      = 100000;
const int          quiesceCycles = 50;
const bool         failMaxCycles = false;
const unsigned int stopOnHang    = 5000000;
const unsigned int stopOnLoop    = 0;
const bool         debugWB       = false;
const bool         debugUART     = false;

const std::string romFile     = "cmod7_kintex_rom.init";

#define UART_BASE       0xFFF04000U
#define UART_RXTX       (UART_BASE + 0x00)
#define UART_TXFULL     (UART_BASE + 0x04)
#define UART_RXEMPTY    (UART_BASE + 0x08)
#define UART_EV_PEND    (UART_BASE + 0x10)
#define UART_EV_EN      (UART_BASE + 0x14)
#define UART_TXEMPTY    (UART_BASE + 0x18)
#define UART_RXFULL     (UART_BASE + 0x1C)

std::queue<unsigned char> uart_rx_queue;

static void uart_poll_stdin() {
    unsigned char c;
    if (::read(STDIN_FILENO, &c, 1) == 1)
        uart_rx_queue.push(c);
}

static bool uart_wb_read(unsigned int addr, unsigned int *data) {
    addr &= 0xFFFFFFFC;
    switch (addr) {
        case UART_RXTX:
            if (uart_rx_queue.empty()) {
                *data = 0;
            } else {
                *data = (unsigned int)uart_rx_queue.front() << 24;
                uart_rx_queue.pop();
            }
            return true;
        case UART_TXFULL:   *data = 0; return true;
        case UART_RXEMPTY:  *data = uart_rx_queue.empty() ? 1 : 0; return true;
        case UART_EV_PEND:  *data = 0; return true;
        case UART_EV_EN:    *data = 0; return true;
        case UART_TXEMPTY:  *data = 1; return true;
        case UART_RXFULL:   *data = 0; return true;
        default: return false;
    }
}

static bool uart_wb_write(unsigned int addr, unsigned int data) {
    addr &= 0xFFFFFFFC;
    if (addr == UART_RXTX) {
        char c = 0;
        if (data & 0xFF000000) c = (char)((data >> 24) & 0xFF);
        else if (data & 0x00FF0000) c = (char)((data >> 16) & 0xFF);
        else if (data & 0x0000FF00) c = (char)((data >> 8) & 0xFF);
        else c = (char)(data & 0xFF);
        if (c != '\r' && c != '\0') std::cout << c << std::flush;
        return true;
    }
    if (addr >= UART_BASE && addr <= UART_RXFULL) return true;
    return false;
}

class Memory {
    std::unordered_map<unsigned int, unsigned int> mem;
public:
    bool logStores;
    int  defaultVal;
    Memory() : logStores(false), defaultVal(0) {}

    void loadFile(const std::string &filename, unsigned int adr = 0) {
        unsigned int dat;
        std::ifstream f(filename);
        if (!f.is_open()) {
            std::cerr << "WARNING: could not open " << filename << std::endl;
            return;
        }
        unsigned int count = 0, base = adr;
        while (f >> std::hex >> dat) { mem[adr] = dat; adr += 4; count++; }
        std::cerr << "Loaded " << std::dec << count << " words from "
                  << filename << " @ 0x" << std::hex << base << std::endl;
    }

    int read(unsigned int adr) {
        auto it = mem.find(adr & ~3u);
        return (it != mem.end()) ? it->second : defaultVal;
    }

    void write(unsigned int adr, unsigned int dat) { mem[adr & ~3u] = dat; }

    void write(unsigned int adr, unsigned int be, unsigned int dat) {
        if (!be) return;
        unsigned int mask = 0;
        if (be & 8) mask |= 0xFF000000;
        if (be & 4) mask |= 0x00FF0000;
        if (be & 2) mask |= 0x0000FF00;
        if (be & 1) mask |= 0x000000FF;
        unsigned int prev = read(adr);
        mem[adr & ~3u] = (prev & ~mask) | (dat & mask);
    }
};

Memory mem;

static struct termios orig_termios;
static void terminal_restore() { tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios); }
static void terminal_raw() {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(terminal_restore);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int main(int argc, char **argv) {
    using namespace std;

    fcntl(STDIN_FILENO, F_SETFL,
          fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    terminal_raw();

    cerr << "=== tb_litex_mp: MicroPython on A2O ===" << endl;
    Verilated::commandArgs(argc, argv);
    m    = new Va2owb;
    root = m->rootp;

#ifdef TRACING
    Verilated::traceEverOn(true);
    t = new VerilatedVcdC;
    m->trace(t, 99);
    t->open("a2olitex_mp.vcd");
    cerr << "Tracing -> a2olitex_mp.vcd" << endl;
#endif

    mem.write(0xFFFFFFFC, 0x48000004);
    mem.loadFile(romFile, 0x00000000);

    bool ok = true, done = false, resetDone = false;
    unsigned int quiesceCount = 0, tick = 0, cycle = 1;
    unsigned int lastCompCycle = 0, lastCompSame = 0;
    bool wbRdPending = false, wbWrPending = false;
    unsigned int wbPendingAddr = 0;
    unsigned int wbPendingData = 0;
    unsigned int wbPendingSel  = 0;
    unsigned int iu0Comp, iu1Comp;
    unsigned long iu0CompIFAR, iu1CompIFAR, iuCompFlushIFAR;

    m->rst = 1;
    cerr << dec << setw(8) << cycle << " Resetting..." << endl;

    const int clocks[2] = {0x1, 0x0};
    const int ticks1x   = 2;

    while (!Verilated::gotFinish() &&
           (ok || quiesceCount > 0) &&
           (cycle <= (unsigned)runCycles || !failMaxCycles) &&
           !done) {

        if (!resetDone && (cycle > resetCycle)) {
            m->rst = 0;
            resetDone = true;
            cerr << dec << setw(8) << cycle << " Releasing reset." << endl;
        }

        if (resetDone && cycle == (unsigned)(resetCycle + 3)) {
            m->cfg_dat = 0x00000000;
            m->cfg_wr  = 1;
        } else if (resetDone && cycle == (unsigned)(resetCycle + 4)) {
            m->cfg_wr  = 0;
        }

        m->clk = clocks[tick % ticks1x];
        m->eval();

        if ((tick % ticks1x) == 0) {
            uart_poll_stdin();

            iu0Comp = root->a2owb->c0->iuq0->iuq_cpl_top0->iuq_cpl0->cp2_i0_completed;
            iu1Comp = root->a2owb->c0->iuq0->iuq_cpl_top0->iuq_cpl0->cp2_i1_completed;

            if (iu0Comp || iu1Comp) {
                lastCompCycle = cycle;
            } else if (!quiesceCount && stopOnHang &&
                       (cycle - lastCompCycle > stopOnHang)) {
                cerr << "*** No completion in " << dec << stopOnHang << " cycles ***" << endl;
                ok = false;
            }

            m->wb_ack = 0;
            if (wbRdPending) {
                unsigned int data = 0;
                if (!uart_wb_read(wbPendingAddr, &data))
                    data = mem.read(wbPendingAddr);
                m->wb_datr = data;
                m->wb_ack  = 1;
                wbRdPending = false;
            } else if (wbWrPending) {
                if (!uart_wb_write(wbPendingAddr, wbPendingData))
                    mem.write(wbPendingAddr, wbPendingSel, wbPendingData);
                m->wb_ack = 1;
                wbWrPending = false;
            } else if (m->wb_cyc && m->wb_stb) {
                wbPendingAddr = m->wb_adr & 0xFFFFFFFC;
                wbPendingData = m->wb_datw;
                wbPendingSel  = m->wb_sel;
                if (!m->wb_we) wbRdPending = true;
                else           wbWrPending = true;
            }
        }

        m->eval();

        if ((tick % ticks1x) == 0) {
            cycle++;
            if ((cycle % hbCycles) == 0)
                cerr << dec << setw(8) << setfill('0') << cycle << " ...tick..." << endl;
            if (failMaxCycles && cycle == (unsigned)runCycles) {
                cerr << "*** Max cycles ***" << endl;
                ok = false;
            }
        }
        tick++;

#ifdef TRACING
        t->dump(tick);
        t->flush();
#endif

        if (!ok && quiesceCount == 0) {
            quiesceCount = quiesceCycles;
            cerr << "Quiescing..." << endl;
        } else if (quiesceCount > 0) {
            quiesceCount--;
            if (ok && quiesceCount == 0) done = true;
        }
    }

#ifdef TRACING
    t->close();
#endif
    m->final();
    terminal_restore();
    cerr << endl << tbName << " Cycles=" << dec << cycle << endl;
    exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
