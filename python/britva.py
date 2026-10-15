#!/usr/bin/env python3
"""Britva OS Python edition: a safe, host-side shell simulation."""

from __future__ import annotations

import argparse
import sys
from typing import Iterable

VERSION = "0.1.0"
PROMPT = "britva@python:~$ "

HELP_LINES = [
    "Commands:",
    "  help                 Show this list",
    "  about                About Britva OS",
    "  version              Show version and architecture",
    "  uname                Print system identity",
    "  whoami               Print current user",
    "  mem                  Show memory status",
    "  echo <text>          Print text",
    "  ls                   List built-in files",
    "  cat <file>           Read a built-in file",
    "  color <name>         Set cyan, green, white, or amber",
    "  clear                Clear the terminal",
    "  reboot               Restart (kernel edition only)",
]

FILES = {
    "readme.txt": [
        "Britva OS: a small 32-bit x86 learning kernel.",
        "Boot path: BIOS -> Assembly -> C++ kernel.",
    ],
    "motd.txt": ["Welcome to Britva OS. Type help to explore."],
}

ANSI_COLORS = {
    "cyan": "\033[96m",
    "green": "\033[92m",
    "white": "\033[97m",
    "amber": "\033[93m",
}
ANSI_RESET = "\033[0m"


class BritvaShell:
    """Small command interpreter shared by the Python CLI and its tests."""

    def __init__(self) -> None:
        self.color = "cyan"

    def execute(self, raw: str) -> list[str]:
        """Run one command and return display lines (clear returns no lines)."""
        stripped = raw.strip()
        if not stripped:
            return []

        command, _, tail = stripped.partition(" ")
        command = command.lower()
        arguments = tail.lstrip()

        if command == "help":
            return list(HELP_LINES)
        if command == "about":
            return [
                "Britva OS is a tiny, open-source x86 hobby operating system.",
                "It boots with GNU Assembly and runs a freestanding C++ kernel.",
                "This Python edition is a safe host-side shell simulation.",
            ]
        if command == "version":
            return [f"Britva OS {VERSION} | i386 | C++ kernel + GNU Assembly bootloader"]
        if command == "uname":
            return [f"Britva {VERSION} i386"]
        if command == "whoami":
            return ["root"]
        if command == "mem":
            return ["Memory map unavailable: the Python simulation does not report host RAM."]
        if command == "echo":
            return [arguments]
        if command == "ls":
            return ["README.txt  MOTD.txt"]
        if command == "cat":
            filename = arguments.lower()
            if filename in FILES:
                return list(FILES[filename])
            return ["Usage: cat README.txt | MOTD.txt"]
        if command == "color":
            color = arguments.lower()
            if color not in ANSI_COLORS:
                return ["Usage: color cyan | green | white | amber"]
            self.color = color
            return ["Console color updated."]
        if command == "clear":
            return []
        if command == "reboot":
            return ["The Python edition cannot reboot the host. Restart it with Ctrl+C."]
        return [f"Unknown command: {command}. Type 'help' for available commands."]

    def _write_line(self, line: str, *, color_enabled: bool) -> None:
        if color_enabled:
            sys.stdout.write(f"{ANSI_COLORS[self.color]}{line}{ANSI_RESET}\n")
        else:
            print(line)

    def run(self) -> int:
        color_enabled = sys.stdout.isatty()
        print("BRITVA OS")
        print(f"Python edition {VERSION} (safe host simulation)")
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
            print("\nLeaving Britva Python edition.")
            return 0


def run_demo(shell: BritvaShell, commands: Iterable[str]) -> None:
    print("BRITVA OS")
    print(f"Python edition {VERSION} (safe host simulation)")
    print("Type 'help' to list commands.\n")
    for command in commands:
        print(f"{PROMPT}{command}")
        for line in shell.execute(command):
            print(line)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-c", "--command", help="run one shell command and exit")
    parser.add_argument("--demo", action="store_true", help="print a short scripted preview")
    parser.add_argument("--version", action="version", version=f"Britva Python {VERSION}")
    args = parser.parse_args(argv)

    shell = BritvaShell()
    if args.command is not None:
        for line in shell.execute(args.command):
            print(line)
        return 0
    if args.demo:
        run_demo(shell, ["about", "echo Hello from Britva", "uname"])
        return 0
    return shell.run()


if __name__ == "__main__":
    raise SystemExit(main())
