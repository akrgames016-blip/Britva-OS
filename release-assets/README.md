# Britva OS v0.1.0 image

The bootable legacy-BIOS floppy image is stored as a gzip-compressed raw image:

- `britva-os-0.1.0.img.gz` — decompresses to a 1,474,560-byte 1.44 MB disk image.
- `SHA256SUMS` — checksum of the decompressed `.img` file.

From the repository root:

```sh
gzip -dc release-assets/britva-os-0.1.0.img.gz > britva-os-0.1.0.img
sha256sum -c release-assets/SHA256SUMS
```

Then attach the raw image as a floppy in a legacy-BIOS VM (for example, `qemu-system-i386 -boot order=a -drive format=raw,file=britva-os-0.1.0.img,if=floppy`). The PNG screenshot previews and SVG sources are in `screenshots/` and are shown in the root README.
