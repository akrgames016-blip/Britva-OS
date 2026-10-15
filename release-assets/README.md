# Bootable images

## Britva OS v0.2.0 (current)

`britva-os-0.2.0.img.gz` expands to a 1,474,560-byte raw 1.44 MB legacy-BIOS floppy image. Its checksum is in `SHA256SUMS-v0.2.0`.

From the repository root:

```sh
gzip -dc release-assets/britva-os-0.2.0.img.gz > britva-os-0.2.0.img
sha256sum -c release-assets/SHA256SUMS-v0.2.0
```

Then boot it in a legacy-BIOS VM, for example:

```sh
qemu-system-i386 -m 64M -boot order=a \
  -drive format=raw,file=britva-os-0.2.0.img,if=floppy -serial stdio
```

## Britva OS v0.1.0 (superseded)

The earlier prototype image remains as `britva-os-0.1.0.img.gz`; it predates the interrupt-driven kernel. Its historical release notes disclose that its screenshot graphics were illustrative mockups. Use v0.2.0 for the actual OS implementation.
