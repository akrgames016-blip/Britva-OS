#!/usr/bin/env python3
"""Britva OS Python shell reference simulator (not a host OS/kernel)."""

from __future__ import annotations

import argparse
import sys
import time
from typing import Iterable

VERSION = "0.2.0"
PROMPT = "britva@python:~$ "

HELP_LINES = [
    "Commands:",
    "  help                 Show this list",
    "  about                About Britva OS",
    "  version              Show version and architecture",
    "  uname                Print system identity",
    "  whoami               Print current user",
    "  mem                  Show BIOS memory and page counts",
    "  uptime               Show time since boot",
    "  palloc               Allocate one 4 KiB physical page",
    "  pfree <hex-address>  Release a page allocated by palloc",
    "  echo <text>          Print text",
    "  ls                   List embedded read-only files",
    "  cat <file>           Read an embedded file",
    "  color <name>         Set cyan, green, white, or amber",
    "  clear                Clear the terminal",
    "  reboot               Restart (kernel edition only)",
    "  halt                 Halt (kernel edition only)",
]

FILES = {
    "readme.txt": [
        "Britva OS v0.2.0",
        "BIOS loader -> protected mode -> C++ kernel -> interrupt-driven shell.",
        "The embedded read-only files are stored in the kernel image.",
    ],
    "motd.txt": ["Welcome to Britva OS. Type help to inspect the shell."],
    "limits.txt": [
        "Legacy BIOS only. No paging, user mode, disk filesystem, or multitasking yet."
    ],
}

ANSI_COLORS = {
    "cyan": "\033[96m",
    "green": "\033[92m",
    "white": "\033[97m",
    "amber": "\033[93m",
}
ANSI_RESET = "\033[0m"


class BritvaShell:
    """Command interpreter mirroring the bare-metal kernel shell."""

    def __init__(self) -> None:
        self.color = "cyan"
        self.started = time.monotonic()
        self.next_page_handle = 0x01000000
        self.allocated_pages: set[int] = set()

    def execute(self, raw: str) -> list[str]:
        """Run a shell command and return its lines; clear returns no lines."""
        parts = raw.strip().split(None, 1)
        if not parts:
            return []
        command = parts[0].lower()
        arguments = parts[1] if len(parts) == 2 else ""

        if command == "help":
            return list(HELP_LINES)
        if command == "about":
            return [
                "Britva OS is a bootable 32-bit x86 hobby operating system.",
                "The BIOS loader starts a C++ kernel with IDT/PIC/PIT and IRQ1/IRQ4 input.",
                "This Python program is a reference shell simulator, not the kernel.",
            ]
        if command == "version":
            return [f"Britva OS {VERSION} | i386 | C++ kernel + GNU Assembly bootloader"]
        if command == "uname":
            return [f"Britva {VERSION} i386"]
        if command == "whoami":
            return ["root"]
        if command == "mem":
            return [
                "BIOS memory values are available only in the bare-metal kernel "
                f"(INT 12h / INT 15h). Simulator page handles: {len(self.allocated_pages)}."
            ]
        if command == "uptime":
            seconds = int(time.monotonic() - self.started)
            return [f"Simulator uptime: {seconds} seconds"]
        if command == "palloc":
            handle = self.next_page_handle
            self.next_page_handle += 4096
            self.allocated_pages.add(handle)
            return [
                f"Simulation-only page handle allocated at 0x{handle:x}; "
                "no host physical memory is reserved."
            ]
        if command == "pfree":
            try:
                handle = int(arguments.strip(), 16)
            except ValueError:
                return ["Usage: pfree <hex-address>"]
            if handle not in self.allocated_pages:
                return ["Unknown simulator page handle."]
            self.allocated_pages.remove(handle)
            return ["Simulator page handle released."]
        if command == "echo":
            return [arguments]
        if command == "ls":
            return ["README.TXT  MOTD.TXT  LIMITS.TXT"]
        if command == "cat":
            filename = arguments.strip().lower()
            if filename in FILES:
                return list(FILES[filename])
            if not filename:
                return ["Usage: cat README.TXT | MOTD.TXT | LIMITS.TXT"]
            return [f"File not found: {arguments.strip()}"]
        if command == "color":
            color = arguments.strip().lower()
            if color not in ANSI_COLORS:
                return ["Usage: color cyan | green | white | amber"]
            self.color = color
            return ["Console color updated."]
        if command == "clear":
            return []
        if command == "reboot":
            return ["The Python simulator cannot reboot the host. Restart it with Ctrl+C."]
        if command == "halt":
            return ["The Python simulator cannot halt the host. Press Ctrl+C to leave."]
        return [f"Unknown command: {command}. Type 'help' for available commands."]

    def _write_line(self, line: str, *, color_enabled: bool) -> None:
        if color_enabled:
            sys.stdout.write(f"{ANSI_COLORS[self.color]}{line}{ANSI_RESET}\n")
        else:
            print(line)

    def run(self) -> int:
        color_enabled = sys.stdout.isatty()
        print("BRITVA OS — Python shell simulator")
        print(f"Reference version {VERSION}; this is not a kernel or virtual machine.")
        print("Type 'help' to list commands. Press Ctrl+C to leave.\n")

        try:
            while True:
                prompt = PROMPT
                if color_enabled:
                    prompt = f"{ANSI_COLORS[self.color]}{PROMPT}{ANSI_RESET}"
                try:
                    raw = input(prompt)
                except EOFError:
                    print()
                    return 0

                if raw.strip().lower() == "clear" and color_enabled:
                    sys.stdout.write("\033[2J\033[H")
                    sys.stdout.flush()
                for line in self.execute(raw):
                    self._write_line(line, color_enabled=color_enabled)
        except KeyboardInterrupt:
            print("\nLeaving Britva Python shell simulator.")
            return 0


def run_demo(shell: BritvaShell, commands: Iterable[str]) -> None:
    print("BRITVA OS — Python shell simulator")
    print(f"Reference version {VERSION}; this is not a kernel or virtual machine.\n")
    for command in commands:
        print(f"{PROMPT}{command}")
        for line in shell.execute(command):
            print(line)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-c", "--command", help="run one shell command and exit")
    parser.add_argument("--demo", action="store_true", help="print a scripted shell example")
    parser.add_argument("--version", action="version", version=f"Britva Python {VERSION}")
    args = parser.parse_args(argv)

    shell = BritvaShell()
    if args.command is not None:
        for line in shell.execute(args.command):
            print(line)
        return 0
    if args.demo:
        run_demo(shell, ["about", "echo Hello from Britva", "uname", "uptime"])
        return 0
    return shell.run()


if __name__ == "__main__":
    raise SystemExit(main())
