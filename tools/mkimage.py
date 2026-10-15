#!/usr/bin/env python3
"""Build a raw 1.44 MB BIOS floppy image for Britva OS."""

from __future__ import annotations

import argparse
from pathlib import Path

SECTOR_SIZE = 512
FLOPPY_SECTORS = 2880
KERNEL_LOAD_SECTORS = 64  # Must match the disk packet in boot/boot.S.


def make_image(boot_path: Path, kernel_path: Path, output_path: Path) -> None:
    boot = boot_path.read_bytes()
    kernel = kernel_path.read_bytes()

    if len(boot) != SECTOR_SIZE:
        raise ValueError(f"boot sector must be exactly {SECTOR_SIZE} bytes; got {len(boot)}")
    if boot[510:512] != b"\x55\xaa":
        raise ValueError("boot sector is missing the BIOS 0x55AA signature")
    max_kernel_bytes = KERNEL_LOAD_SECTORS * SECTOR_SIZE
    if len(kernel) > max_kernel_bytes:
        raise ValueError(
            f"kernel is {len(kernel)} bytes; bootloader can load at most {max_kernel_bytes}"
        )

    image = bytearray(FLOPPY_SECTORS * SECTOR_SIZE)
    image[:SECTOR_SIZE] = boot
    image[SECTOR_SIZE : SECTOR_SIZE + len(kernel)] = kernel
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(image)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("boot_sector", type=Path)
    parser.add_argument("kernel", type=Path)
    parser.add_argument("output_image", type=Path)
    args = parser.parse_args()

    try:
        make_image(args.boot_sector, args.kernel, args.output_image)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(f"Created {args.output_image} ({FLOPPY_SECTORS * SECTOR_SIZE:,} bytes)")
    print(f"  boot sector: {args.boot_sector.stat().st_size} bytes")
    print(f"  kernel:      {args.kernel.stat().st_size} bytes at LBA 1")
    print(f"  load window: {KERNEL_LOAD_SECTORS} sectors / {KERNEL_LOAD_SECTORS * SECTOR_SIZE:,} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
