/*
 * Britva OS v0.1.0 — freestanding 32-bit x86 kernel.
 * No C/C++ runtime, libc, or heap is required. Text goes to VGA and COM1;
 * input is polled from the PS/2 keyboard and serial port.
 */

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

static const u16 kCom1 = 0x3f8;
static const u16 kVgaWidth = 80;
static const u16 kVgaHeight = 25;
static const u16 kInputCapacity = 128;
static volatile u16* const kVga = reinterpret_cast<volatile u16*>(0xb8000);

static u16 g_cursor_x = 0;
static u16 g_cursor_y = 0;
static u8 g_color = 0x0b;
static char g_input[kInputCapacity];
static bool g_shift_down = false;
static bool g_serial_present = false;
static bool g_discard_lf_after_cr = false;

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_init() {
    outb(kCom1 + 1, 0x00);  // Disable UART interrupts.
    outb(kCom1 + 3, 0x80);  // Enable divisor latch.
    outb(kCom1 + 0, 0x01);  // 115200 baud (divisor 1).
    outb(kCom1 + 1, 0x00);
    outb(kCom1 + 3, 0x03);  // 8 data bits, no parity, one stop bit.
    outb(kCom1 + 2, 0xc7);  // Enable and clear FIFO.
    outb(kCom1 + 4, 0x1e);  // Loopback mode for a simple UART presence test.
    outb(kCom1 + 0, 0xae);
    g_serial_present = inb(kCom1 + 0) == 0xae;
    if (!g_serial_present) {
        outb(kCom1 + 4, 0x00);
        return;
    }
    outb(kCom1 + 4, 0x0b);  // DTR, RTS, OUT2; leave loopback mode.
}

static void serial_put(char value) {
    if (!g_serial_present) {
        return;
    }
    u32 timeout = 100000;
    while ((inb(kCom1 + 5) & 0x20) == 0 && --timeout != 0) {
    }
    if (timeout != 0) {
        outb(kCom1, static_cast<u8>(value));
    }
}

static void serial_write(const char* text) {
    while (*text != '\0') {
        if (*text == '\n') {
            serial_put('\r');
        }
        serial_put(*text++);
    }
}

static void scroll_if_needed() {
    if (g_cursor_y < kVgaHeight) {
        return;
    }

    for (u16 y = 1; y < kVgaHeight; ++y) {
        for (u16 x = 0; x < kVgaWidth; ++x) {
            kVga[(y - 1) * kVgaWidth + x] = kVga[y * kVgaWidth + x];
        }
    }
    const u16 blank = static_cast<u16>((static_cast<u16>(g_color) << 8) | ' ');
    for (u16 x = 0; x < kVgaWidth; ++x) {
        kVga[(kVgaHeight - 1) * kVgaWidth + x] = blank;
    }
    g_cursor_y = kVgaHeight - 1;
}

static void console_put(char value) {
    if (value == '\n') {
        g_cursor_x = 0;
        ++g_cursor_y;
        serial_put('\r');
        serial_put('\n');
    } else if (value == '\r') {
        g_cursor_x = 0;
        serial_put('\r');
    } else if (value == '\b') {
        if (g_cursor_x > 0) {
            --g_cursor_x;
            kVga[g_cursor_y * kVgaWidth + g_cursor_x] =
                static_cast<u16>((static_cast<u16>(g_color) << 8) | ' ');
        }
        serial_put('\b');
        serial_put(' ');
        serial_put('\b');
    } else {
        if (value == '\t') {
            value = ' ';
        }
        kVga[g_cursor_y * kVgaWidth + g_cursor_x] =
            static_cast<u16>((static_cast<u16>(g_color) << 8) | static_cast<u8>(value));
        ++g_cursor_x;
        if (g_cursor_x >= kVgaWidth) {
            g_cursor_x = 0;
            ++g_cursor_y;
        }
        serial_put(value);
    }
    scroll_if_needed();
}

static void write(const char* text) {
    while (*text != '\0') {
        console_put(*text++);
    }
}

static void write_line(const char* text) {
    write(text);
    console_put('\n');
}

static void clear_screen() {
    const u16 blank = static_cast<u16>((static_cast<u16>(g_color) << 8) | ' ');
    for (u32 i = 0; i < static_cast<u32>(kVgaWidth) * kVgaHeight; ++i) {
        kVga[i] = blank;
    }
    g_cursor_x = 0;
    g_cursor_y = 0;
    serial_write("\x1b[2J\x1b[H");
}

static bool text_equals(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

static void lowercase(char* text) {
    while (*text != '\0') {
        *text = ascii_lower(*text);
        ++text;
    }
}

static bool is_space(char value) {
    return value == ' ' || value == '\t';
}

static void show_help() {
    write_line("Commands:");
    write_line("  help                 Show this list");
    write_line("  about                About Britva OS");
    write_line("  version              Show version and architecture");
    write_line("  uname                Print system identity");
    write_line("  whoami               Print current user");
    write_line("  mem                  Show memory status");
    write_line("  echo <text>          Print text");
    write_line("  ls                   List built-in files");
    write_line("  cat <file>           Read a built-in file");
    write_line("  color <name>         Set cyan, green, white, or amber");
    write_line("  clear                Clear the console");
    write_line("  reboot               Restart the machine");
}

static void execute_command(char* line) {
    while (is_space(*line)) {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    char* command_end = line;
    while (*command_end != '\0' && !is_space(*command_end)) {
        *command_end = ascii_lower(*command_end);
        ++command_end;
    }
    if (*command_end != '\0') {
        *command_end++ = '\0';
    }
    while (is_space(*command_end)) {
        ++command_end;
    }
    char* args = command_end;
    char* args_end = args;
    while (*args_end != '\0') {
        ++args_end;
    }
    while (args_end > args && is_space(args_end[-1])) {
        *--args_end = '\0';
    }

    if (text_equals(line, "help")) {
        show_help();
    } else if (text_equals(line, "about")) {
        write_line("Britva OS is a tiny, open-source x86 hobby operating system.");
        write_line("It boots with GNU Assembly and runs a freestanding C++ kernel.");
        write_line("This early build has a VGA text console and a polling shell.");
    } else if (text_equals(line, "version")) {
        write_line("Britva OS 0.1.0 | i386 | C++ kernel + GNU Assembly bootloader");
    } else if (text_equals(line, "uname")) {
        write_line("Britva 0.1.0 i386");
    } else if (text_equals(line, "whoami")) {
        write_line("root");
    } else if (text_equals(line, "mem")) {
        write_line("Memory map unavailable: this build does not yet pass BIOS RAM data.");
    } else if (text_equals(line, "echo")) {
        write_line(args);
    } else if (text_equals(line, "ls")) {
        write_line("README.txt  MOTD.txt");
    } else if (text_equals(line, "cat")) {
        lowercase(args);
        if (text_equals(args, "readme.txt")) {
            write_line("Britva OS: a small 32-bit x86 learning kernel.");
            write_line("Boot path: BIOS -> Assembly -> C++ kernel.");
        } else if (text_equals(args, "motd.txt")) {
            write_line("Welcome to Britva OS. Type help to explore.");
        } else {
            write_line("Usage: cat README.txt | MOTD.txt");
        }
    } else if (text_equals(line, "color")) {
        lowercase(args);
        if (text_equals(args, "cyan")) {
            g_color = 0x0b;
        } else if (text_equals(args, "green")) {
            g_color = 0x0a;
        } else if (text_equals(args, "white")) {
            g_color = 0x0f;
        } else if (text_equals(args, "amber")) {
            g_color = 0x0e;
        } else {
            write_line("Usage: color cyan | green | white | amber");
            return;
        }
        write_line("Console color updated.");
    } else if (text_equals(line, "clear")) {
        clear_screen();
    } else if (text_equals(line, "reboot")) {
        write_line("Restarting...");
        u32 timeout = 1000000;
        while ((inb(0x64) & 0x02) != 0 && --timeout != 0) {
        }
        outb(0x64, 0xfe);
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    } else {
        write("Unknown command: ");
        write(line);
        write_line(". Type 'help' for available commands.");
    }

}

static char shifted_symbol(char value) {
    switch (value) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case '\\': return '|';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return value;
    }
}

static char key_to_ascii(u8 code, bool shifted) {
    char value = 0;
    switch (code) {
        case 0x02: value = '1'; break;
        case 0x03: value = '2'; break;
        case 0x04: value = '3'; break;
        case 0x05: value = '4'; break;
        case 0x06: value = '5'; break;
        case 0x07: value = '6'; break;
        case 0x08: value = '7'; break;
        case 0x09: value = '8'; break;
        case 0x0a: value = '9'; break;
        case 0x0b: value = '0'; break;
        case 0x0c: value = '-'; break;
        case 0x0d: value = '='; break;
        case 0x10: value = 'q'; break;
        case 0x11: value = 'w'; break;
        case 0x12: value = 'e'; break;
        case 0x13: value = 'r'; break;
        case 0x14: value = 't'; break;
        case 0x15: value = 'y'; break;
        case 0x16: value = 'u'; break;
        case 0x17: value = 'i'; break;
        case 0x18: value = 'o'; break;
        case 0x19: value = 'p'; break;
        case 0x1a: value = '['; break;
        case 0x1b: value = ']'; break;
        case 0x1e: value = 'a'; break;
        case 0x1f: value = 's'; break;
        case 0x20: value = 'd'; break;
        case 0x21: value = 'f'; break;
        case 0x22: value = 'g'; break;
        case 0x23: value = 'h'; break;
        case 0x24: value = 'j'; break;
        case 0x25: value = 'k'; break;
        case 0x26: value = 'l'; break;
        case 0x27: value = ';'; break;
        case 0x28: value = '\''; break;
        case 0x29: value = '`'; break;
        case 0x2b: value = '\\'; break;
        case 0x2c: value = 'z'; break;
        case 0x2d: value = 'x'; break;
        case 0x2e: value = 'c'; break;
        case 0x2f: value = 'v'; break;
        case 0x30: value = 'b'; break;
        case 0x31: value = 'n'; break;
        case 0x32: value = 'm'; break;
        case 0x33: value = ','; break;
        case 0x34: value = '.'; break;
        case 0x35: value = '/'; break;
        case 0x39: value = ' '; break;
        default: return 0;
    }
    if (shifted) {
        if (value >= 'a' && value <= 'z') {
            return static_cast<char>(value - 'a' + 'A');
        }
        return shifted_symbol(value);
    }
    return value;
}

static void submit_input(u16* length) {
    g_input[*length] = '\0';
    console_put('\n');
    execute_command(g_input);
    *length = 0;
}

static void read_line() {
    u16 length = 0;
    for (;;) {
        if (g_serial_present && (inb(kCom1 + 5) & 0x01) != 0) {
            char value = static_cast<char>(inb(kCom1));
            if (value == '\n' && g_discard_lf_after_cr) {
                g_discard_lf_after_cr = false;
                continue;
            }
            if (value == '\r') {
                g_discard_lf_after_cr = true;
                submit_input(&length);
                return;
            }
            if (value == '\n') {
                submit_input(&length);
                return;
            }
            g_discard_lf_after_cr = false;
            if (value == '\b' || static_cast<u8>(value) == 0x7f) {
                if (length > 0) {
                    --length;
                    console_put('\b');
                }
            } else if (value >= 0x20 && value <= 0x7e && length < kInputCapacity - 1) {
                g_input[length++] = value;
                console_put(value);
            }
            continue;
        }

        if ((inb(0x64) & 0x01) == 0) {
            continue;
        }
        const u8 code = inb(0x60);
        if (code == 0x2a || code == 0x36) {
            g_shift_down = true;
            continue;
        }
        if (code == 0xaa || code == 0xb6) {
            g_shift_down = false;
            continue;
        }
        if ((code & 0x80) != 0) {
            continue;
        }
        if (code == 0x1c) {
            submit_input(&length);
        return;
        }
        if (code == 0x0e) {
            if (length > 0) {
                --length;
                console_put('\b');
            }
            continue;
        }
        const char value = key_to_ascii(code, g_shift_down);
        if (value >= 0x20 && value <= 0x7e && length < kInputCapacity - 1) {
            g_input[length++] = value;
            console_put(value);
        }
    }
}

static void show_prompt() {
    write("britva@kernel:~$ ");
}

extern "C" void kernel_main() {
    serial_init();
    clear_screen();
    write_line("BRITVA OS 0.1.0");
    write_line("32-bit x86 | freestanding C++ kernel | GNU Assembly boot");
    write_line("VGA text console + COM1 serial console");
    write_line("Memory map unavailable in this early build.");
    write_line("Type 'help' to list commands.");
    console_put('\n');

    for (;;) {
        show_prompt();
        read_line();
    }
}
