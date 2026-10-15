#!/usr/bin/env python3
"""Boot the raw image in QEMU and exercise the serial shell."""

from __future__ import annotations

import argparse
import os
import selectors
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGE = ROOT / "build" / "britva-os.img"


def smoke_test(image: Path, qemu: str) -> None:
    if not image.is_file():
        raise FileNotFoundError(f"boot image not found: {image}; run `make` first")

    command = [
        qemu,
        "-accel", "tcg",
        "-m", "64M",
        "-boot", "order=a",
        "-drive", f"file={image},format=raw,if=floppy",
        "-display", "none",
        "-monitor", "none",
        "-serial", "stdio",
        "-no-reboot",
        "-no-shutdown",
    ]
    process = subprocess.Popen(
        command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )
    assert process.stdin is not None
    assert process.stdout is not None

    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    output = bytearray()

    def wait_for(needle: bytes, phase: str, timeout: float = 12.0) -> None:
        deadline = time.monotonic() + timeout
        while needle not in output:
            if process.poll() is not None:
                raise RuntimeError(
                    f"QEMU exited during {phase} (status {process.returncode}); "
                    f"serial output: {output.decode(errors='replace')}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    f"timed out during {phase}; serial output: {output.decode(errors='replace')}"
                )
            events = selector.select(remaining)
            if not events:
                continue
            chunk = os.read(process.stdout.fileno(), 4096)
            if not chunk:
                raise RuntimeError(f"QEMU serial stream closed during {phase}")
            output.extend(chunk)

    try:
        wait_for(b"britva@kernel:~$ ", "kernel boot")
        process.stdin.write(b"echo BOOT_OK\r")
        process.stdin.flush()
        wait_for(b"BOOT_OK", "echo command")
        process.stdin.write(b"uname\r")
        process.stdin.flush()
        wait_for(b"Britva 0.1.0 i386", "uname command")
    finally:
        selector.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)

    print("QEMU boot smoke test passed: prompt, echo, and uname responded over COM1.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--require", action="store_true", help="fail if QEMU is not installed")
    args = parser.parse_args()

    qemu = shutil.which("qemu-system-i386")
    if qemu is None:
        message = "qemu-system-i386 not found; skipping boot smoke test"
        print(message, file=sys.stderr)
        return 2 if args.require else 0

    try:
        smoke_test(args.image, qemu)
    except (OSError, RuntimeError, TimeoutError) as error:
        print(f"QEMU smoke test failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
