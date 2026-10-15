/*
 * Britva OS v0.2.0 — freestanding 32-bit x86 kernel.
 * No C/C++ runtime, libc, or heap is required. It configures an IDT, remaps
 * the PIC, drives a PIT timer and IRQ1 keyboard, and provides VGA/COM1 I/O.
 */

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

static const u16 kCom1 = 0x3f8;
static const u16 kVgaWidth = 80;
static const u16 kVgaHeight = 25;
static const u16 kInputCapacity = 128;
static const u8 kKeyQueueCapacity = 128;
static volatile u16* const kVga = reinterpret_cast<volatile u16*>(0xb8000);

static u16 g_cursor_x = 0;
static u16 g_cursor_y = 0;
static u8 g_color = 0x0b;
static char g_input[kInputCapacity];
static bool g_shift_down = false;
static bool g_serial_present = false;
static bool g_discard_lf_after_cr = false;
static volatile u32 g_ticks = 0;
static volatile u8 g_key_head = 0;
static volatile u8 g_key_tail = 0;
static volatile u8 g_key_queue[kKeyQueueCapacity];
static volatile u8 g_serial_head = 0;
static volatile u8 g_serial_tail = 0;
static volatile u8 g_serial_queue[kKeyQueueCapacity];
static const u32 kPageSize = 4096;
static const u32 kMaximumFrames = 32768;  // 128 MiB bitmap ceiling.
static u32 g_frame_bitmap[kMaximumFrames / 32];
static u32 g_total_frames = 0;
static u32 g_free_frames = 0;
static u32 g_first_managed_frame = 0;

extern "C" u8 __kernel_end;

struct __attribute__((packed)) IdtEntry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
};

struct __attribute__((packed)) IdtPointer {
    u16 limit;
    u32 base;
};

struct RamFile {
    const char* name;
    const char* contents;
};

static IdtEntry g_idt[256];
static IdtPointer g_idt_pointer;
static const RamFile g_ram_files[] = {
    {"README.TXT", "Britva OS v0.2.0\nA real-mode BIOS loader starts this 32-bit C++ kernel.\nThe kernel handles CPU exceptions, timer IRQs, keyboard IRQs, VGA, and COM1.\n"},
    {"MOTD.TXT", "Welcome to Britva OS. Type help to inspect the shell.\n"},
    {"LIMITS.TXT", "Legacy BIOS only. No paging, user mode, disk filesystem, or multitasking yet.\n"},
};

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();
extern "C" void irq0_stub();
extern "C" void irq1_stub();
extern "C" void irq4_stub();

static void (*const g_exception_stubs[32])() = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void io_wait() {
    outb(0x80, 0);
}

static void serial_init() {
    outb(kCom1 + 1, 0x00);  // Disable UART interrupts.
    outb(kCom1 + 3, 0x80);  // Enable divisor latch.
    outb(kCom1 + 0, 0x01);  // 115200 baud (divisor 1).
    outb(kCom1 + 1, 0x00);
    outb(kCom1 + 3, 0x03);  // 8 data bits, no parity, one stop bit.
    outb(kCom1 + 2, 0xc7);  // Enable and clear FIFO.
    outb(kCom1 + 4, 0x1e);  // Loopback mode for a UART presence test.
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

static void update_hardware_cursor() {
    const u16 position = static_cast<u16>(g_cursor_y * kVgaWidth + g_cursor_x);
    outb(0x3d4, 0x0f);
    outb(0x3d5, static_cast<u8>(position & 0xff));
    outb(0x3d4, 0x0e);
    outb(0x3d5, static_cast<u8>(position >> 8));
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
    update_hardware_cursor();
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

static void write_u32(u32 value) {
    char digits[10];
    u8 length = 0;
    do {
        digits[length++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0 && length < sizeof(digits));

    while (length > 0) {
        console_put(digits[--length]);
    }
}

static void write_hex(u32 value) {
    const char digits[] = "0123456789abcdef";
    bool started = false;
    for (int shift = 28; shift >= 0; shift -= 4) {
        const u8 digit = static_cast<u8>((value >> shift) & 0x0f);
        if (digit != 0 || started || shift == 0) {
            console_put(digits[digit]);
            started = true;
        }
    }
}

static bool parse_hex(const char* text, u32* result) {
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }
    if (*text == '\0') {
        return false;
    }
    u32 value = 0;
    while (*text != '\0') {
        u8 digit;
        if (*text >= '0' && *text <= '9') {
            digit = static_cast<u8>(*text - '0');
        } else if (*text >= 'a' && *text <= 'f') {
            digit = static_cast<u8>(*text - 'a' + 10);
        } else if (*text >= 'A' && *text <= 'F') {
            digit = static_cast<u8>(*text - 'A' + 10);
        } else {
            return false;
        }
        if (value > (0xffffffffu - digit) / 16u) {
            return false;
        }
        value = (value << 4) | digit;
        ++text;
    }
    *result = value;
    return true;
}

static u16 read_boot_word(u32 address) {
    u16 value;
    __asm__ volatile("movw (%1), %0" : "=r"(value) : "r"(address) : "memory");
    return value;
}

static void initialize_page_allocator() {
    for (u32 i = 0; i < kMaximumFrames / 32; ++i) {
        g_frame_bitmap[i] = 0xffffffffu;
    }

    const u32 extended_kib = read_boot_word(0x0502);
    u32 memory_bytes = 0x00100000u + extended_kib * 1024u;
    const u32 bitmap_limit = kMaximumFrames * kPageSize;
    if (memory_bytes > bitmap_limit) {
        memory_bytes = bitmap_limit;
    }
    g_total_frames = memory_bytes / kPageSize;

    const u32 kernel_end = reinterpret_cast<u32>(&__kernel_end);
    u32 first_frame = (kernel_end + kPageSize - 1) / kPageSize;
    if (first_frame < 0x00100000u / kPageSize) {
        first_frame = 0x00100000u / kPageSize;
    }
    if (first_frame > g_total_frames) {
        first_frame = g_total_frames;
    }
    g_first_managed_frame = first_frame;
    g_free_frames = g_total_frames - first_frame;

    for (u32 frame = first_frame; frame < g_total_frames; ++frame) {
        g_frame_bitmap[frame / 32] &= ~(1u << (frame % 32));
    }
}

static u32 allocate_page() {
    if (g_free_frames == 0) {
        return 0;
    }
    for (u32 frame = g_first_managed_frame; frame < g_total_frames; ++frame) {
        const u32 mask = 1u << (frame % 32);
        if ((g_frame_bitmap[frame / 32] & mask) == 0) {
            g_frame_bitmap[frame / 32] |= mask;
            --g_free_frames;
            return frame * kPageSize;
        }
    }
    g_free_frames = 0;
    return 0;
}

static bool release_page(u32 address) {
    if ((address & (kPageSize - 1)) != 0) {
        return false;
    }
    const u32 frame = address / kPageSize;
    if (frame < g_first_managed_frame || frame >= g_total_frames) {
        return false;
    }
    const u32 mask = 1u << (frame % 32);
    if ((g_frame_bitmap[frame / 32] & mask) == 0) {
        return false;
    }
    g_frame_bitmap[frame / 32] &= ~mask;
    ++g_free_frames;
    return true;
}

static void clear_screen() {
    const u16 blank = static_cast<u16>((static_cast<u16>(g_color) << 8) | ' ');
    for (u32 i = 0; i < static_cast<u32>(kVgaWidth) * kVgaHeight; ++i) {
        kVga[i] = blank;
    }
    g_cursor_x = 0;
    g_cursor_y = 0;
    update_hardware_cursor();
    serial_write("\x1b[2J\x1b[H");
}

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

static bool text_equals(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        if (ascii_lower(*left) != ascii_lower(*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool is_space(char value) {
    return value == ' ' || value == '\t';
}

static void set_idt_gate(u8 index, void (*handler)()) {
    const u32 address = reinterpret_cast<u32>(handler);
    g_idt[index].offset_low = static_cast<u16>(address & 0xffff);
    g_idt[index].selector = 0x08;
    g_idt[index].zero = 0;
    g_idt[index].type_attributes = 0x8e;  // Present, ring 0, 32-bit interrupt gate.
    g_idt[index].offset_high = static_cast<u16>(address >> 16);
}

static void initialize_idt() {
    for (u16 index = 0; index < 256; ++index) {
        g_idt[index].offset_low = 0;
        g_idt[index].selector = 0;
        g_idt[index].zero = 0;
        g_idt[index].type_attributes = 0;
        g_idt[index].offset_high = 0;
    }
    for (u8 index = 0; index < 32; ++index) {
        set_idt_gate(index, g_exception_stubs[index]);
    }
    set_idt_gate(32, irq0_stub);
    set_idt_gate(33, irq1_stub);
    set_idt_gate(36, irq4_stub);

    g_idt_pointer.limit = static_cast<u16>(sizeof(g_idt) - 1);
    g_idt_pointer.base = reinterpret_cast<u32>(g_idt);
    __asm__ volatile("lidt %0" : : "m"(g_idt_pointer));
}

static void initialize_pic() {
    outb(0x20, 0x11);
    io_wait();
    outb(0xa0, 0x11);
    io_wait();
    outb(0x21, 0x20);  // Master IRQs start at IDT vector 32.
    io_wait();
    outb(0xa1, 0x28);  // Slave IRQs start at IDT vector 40.
    io_wait();
    outb(0x21, 0x04);  // Slave is connected to master IRQ2.
    io_wait();
    outb(0xa1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xa1, 0x01);
    io_wait();
    outb(0x21, g_serial_present ? 0xec : 0xfc);  // IRQ0 timer, IRQ1 keyboard, IRQ4 COM1.
    outb(0xa1, 0xff);
}

static void initialize_pit() {
    const u16 divisor = static_cast<u16>(1193182 / 100);  // 100 timer ticks per second.
    outb(0x43, 0x36);
    outb(0x40, static_cast<u8>(divisor & 0xff));
    outb(0x40, static_cast<u8>(divisor >> 8));
}

static void initialize_interrupts() {
    __asm__ volatile("cli");
    initialize_idt();
    initialize_pic();
    initialize_pit();
    if (g_serial_present) {
        outb(kCom1 + 1, 0x01);  // Receive-data interrupt on IRQ4.
    }
    __asm__ volatile("sti");
}

extern "C" void exception_handler(u32 vector) {
    __asm__ volatile("cli");
    write_line("");
    write("FATAL: CPU exception vector ");
    write_u32(vector);
    write_line(". System halted.");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

extern "C" void irq0_handler() {
    ++g_ticks;
    outb(0x20, 0x20);
}

extern "C" void irq1_handler() {
    const u8 scan_code = inb(0x60);
    const u8 next = static_cast<u8>((g_key_head + 1) & (kKeyQueueCapacity - 1));
    if (next != g_key_tail) {
        g_key_queue[g_key_head] = scan_code;
        g_key_head = next;
    }
    outb(0x20, 0x20);
}

extern "C" void irq4_handler() {
    while ((inb(kCom1 + 5) & 0x01) != 0) {
        const u8 value = inb(kCom1);
        const u8 next = static_cast<u8>((g_serial_head + 1) & (kKeyQueueCapacity - 1));
        if (next != g_serial_tail) {
            g_serial_queue[g_serial_head] = value;
            g_serial_head = next;
        }
    }
    outb(0x20, 0x20);
}

static bool pop_serial_character(char* value) {
    const u8 tail = g_serial_tail;
    if (tail == g_serial_head) {
        return false;
    }
    *value = static_cast<char>(g_serial_queue[tail]);
    g_serial_tail = static_cast<u8>((tail + 1) & (kKeyQueueCapacity - 1));
    return true;
}

static bool pop_scan_code(u8* scan_code) {
    const u8 tail = g_key_tail;
    if (tail == g_key_head) {
        return false;
    }
    *scan_code = g_key_queue[tail];
    g_key_tail = static_cast<u8>((tail + 1) & (kKeyQueueCapacity - 1));
    return true;
}

static void show_help() {
    write_line("Commands:");
    write_line("  help                 Show this list");
    write_line("  about                About Britva OS");
    write_line("  version              Show version and architecture");
    write_line("  uname                Print system identity");
    write_line("  whoami               Print current user");
    write_line("  mem                  Show BIOS memory and page counts");
    write_line("  uptime               Show time since boot");
    write_line("  palloc               Allocate one 4 KiB physical page");
    write_line("  pfree <hex-address>  Release a page allocated by palloc");
    write_line("  echo <text>          Print text");
    write_line("  ls                   List embedded read-only files");
    write_line("  cat <file>           Read an embedded file");
    write_line("  color <name>         Set cyan, green, white, or amber");
    write_line("  clear                Clear the console");
    write_line("  reboot               Restart the machine");
    write_line("  halt                 Halt the CPU");
}

static void command_memory() {
    const u16 conventional_kib = read_boot_word(0x0500);
    const u16 extended_kib = read_boot_word(0x0502);
    write("Conventional memory (INT 12h): ");
    write_u32(conventional_kib);
    write_line(" KiB");

    write("Extended memory above 1 MiB (INT 15h/88h): ");
    if (extended_kib == 0) {
        write_line("unavailable");
    } else {
        write_u32(extended_kib);
        write_line(" KiB");
    }
    write("Free physical pages (4 KiB): ");
    write_u32(g_free_frames);
    write(" / ");
    write_u32(g_total_frames - g_first_managed_frame);
    write_line("");
}

static void command_page_allocate() {
    const u32 address = allocate_page();
    if (address == 0) {
        write_line("No free page frames reported by the BIOS allocator.");
        return;
    }
    write("Allocated 4 KiB page at 0x");
    write_hex(address);
    write_line("");
}

static void command_page_free(const char* argument) {
    u32 address;
    if (!parse_hex(argument, &address)) {
        write_line("Usage: pfree <hex-address>");
        return;
    }
    if (!release_page(address)) {
        write_line("Address is not an allocated managed page.");
        return;
    }
    write_line("Physical page released.");
}

static void command_uptime() {
    const u32 ticks = g_ticks;
    write("Uptime: ");
    write_u32(ticks / 100);
    write_line(" seconds");
}

static void command_cat(const char* filename) {
    if (*filename == '\0') {
        write_line("Usage: cat README.TXT | MOTD.TXT | LIMITS.TXT");
        return;
    }
    for (u32 i = 0; i < sizeof(g_ram_files) / sizeof(g_ram_files[0]); ++i) {
        if (text_equals(filename, g_ram_files[i].name)) {
            write(g_ram_files[i].contents);
            return;
        }
    }
    write("File not found: ");
    write_line(filename);
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
        write_line("Britva OS is a bootable 32-bit x86 hobby operating system.");
        write_line("BIOS loader -> protected mode -> C++ kernel -> interrupt-driven shell.");
        write_line("The embedded read-only files are stored in the kernel image.");
    } else if (text_equals(line, "version")) {
        write_line("Britva OS 0.2.0 | i386 | C++ kernel + GNU Assembly bootloader");
    } else if (text_equals(line, "uname")) {
        write_line("Britva 0.2.0 i386");
    } else if (text_equals(line, "whoami")) {
        write_line("root");
    } else if (text_equals(line, "mem")) {
        command_memory();
    } else if (text_equals(line, "uptime")) {
        command_uptime();
    } else if (text_equals(line, "palloc")) {
        command_page_allocate();
    } else if (text_equals(line, "pfree")) {
        command_page_free(args);
    } else if (text_equals(line, "echo")) {
        write_line(args);
    } else if (text_equals(line, "ls")) {
        for (u32 i = 0; i < sizeof(g_ram_files) / sizeof(g_ram_files[0]); ++i) {
            write(g_ram_files[i].name);
            if (i + 1 < sizeof(g_ram_files) / sizeof(g_ram_files[0])) {
                write("  ");
            }
        }
        console_put('\n');
    } else if (text_equals(line, "cat")) {
        command_cat(args);
    } else if (text_equals(line, "color")) {
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
    } else if (text_equals(line, "halt")) {
        write_line("CPU halted. Reset the virtual machine to continue.");
        __asm__ volatile("cli");
        for (;;) {
            __asm__ volatile("hlt");
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

static bool handle_serial_character(char value, u16* length) {
    if (value == '\n' && g_discard_lf_after_cr) {
        g_discard_lf_after_cr = false;
        return false;
    }
    if (value == '\r') {
        g_discard_lf_after_cr = true;
        submit_input(length);
        return true;
    }
    if (value == '\n') {
        submit_input(length);
        return true;
    }
    g_discard_lf_after_cr = false;
    if (value == '\b' || static_cast<u8>(value) == 0x7f) {
        if (*length > 0) {
            --*length;
            console_put('\b');
        }
    } else if (value >= 0x20 && value <= 0x7e && *length < kInputCapacity - 1) {
        g_input[(*length)++] = value;
        console_put(value);
    }
    return false;
}

static void read_line() {
    u16 length = 0;
    for (;;) {
        char serial_character;
        if (pop_serial_character(&serial_character)) {
            if (handle_serial_character(serial_character, &length)) {
                return;
            }
            continue;
        }

        u8 code;
        if (!pop_scan_code(&code)) {
            __asm__ volatile("sti; hlt" ::: "memory");
            continue;
        }

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
    initialize_page_allocator();
    initialize_interrupts();

    write_line("BRITVA OS 0.2.0");
    write_line("32-bit x86 protected mode | freestanding C++ kernel");
    write_line("IDT + PIC + 100 Hz PIT + IRQ0 timer + IRQ1 keyboard");
    write_line("IRQ4 COM1 serial input + bitmap page allocator");
    write_line("VGA text console + COM1 serial output");
    write_line("Embedded read-only RAM files: README.TXT, MOTD.TXT, LIMITS.TXT");
    write_line("Type 'help' to list commands.");
    console_put('\n');

    for (;;) {
        show_prompt();
        read_line();
    }
}
