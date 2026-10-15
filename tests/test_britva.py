from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(ROOT / "tools"))

from britva import BritvaShell, VERSION  # noqa: E402
from mkimage import FLOPPY_SECTORS, SECTOR_SIZE, make_image  # noqa: E402


class BritvaShellTests(unittest.TestCase):
    def setUp(self) -> None:
        self.shell = BritvaShell()

    def test_help_lists_supported_commands(self) -> None:
        result = self.shell.execute("help")
        self.assertIn("  about                About Britva OS", result)
        self.assertIn("  uptime               Show time since boot", result)
        self.assertIn("  halt                 Halt (kernel edition only)", result)

    def test_identity_commands(self) -> None:
        self.assertEqual(self.shell.execute("version"), [
            f"Britva OS {VERSION} | i386 | C++ kernel + GNU Assembly bootloader"
        ])
        self.assertEqual(self.shell.execute("uname"), [f"Britva {VERSION} i386"])
        self.assertEqual(self.shell.execute("whoami"), ["root"])

    def test_uptime_is_reported_by_the_python_simulator(self) -> None:
        result = self.shell.execute("uptime")
        self.assertRegex(result[0], r"^Simulator uptime: \d+ seconds$")

    def test_page_commands_are_explicitly_simulated(self) -> None:
        result = self.shell.execute("palloc")[0]
        self.assertIn("no host physical memory is reserved", result)
        self.assertIn("Simulator page handles: 1", self.shell.execute("mem")[0])
        self.assertEqual(self.shell.execute("pfree 0x1000000"), ["Simulator page handle released."])
        self.assertIn("Unknown simulator page handle", self.shell.execute("pfree 0x1000000")[0])

    def test_echo_preserves_text(self) -> None:
        self.assertEqual(self.shell.execute("echo hello, Britva!"), ["hello, Britva!"])
        self.assertEqual(self.shell.execute("echo"), [""])

    def test_commands_are_case_insensitive(self) -> None:
        self.assertEqual(self.shell.execute("UNAME"), [f"Britva {VERSION} i386"])

    def test_virtual_files(self) -> None:
        self.assertEqual(self.shell.execute("ls"), ["README.TXT  MOTD.TXT  LIMITS.TXT"])
        self.assertIn("BIOS loader -> protected mode", self.shell.execute("cat readme.txt")[1])
        self.assertEqual(self.shell.execute("cat missing.txt"), ["File not found: missing.txt"])

    def test_color_updates_only_for_supported_names(self) -> None:
        self.assertEqual(self.shell.execute("color green"), ["Console color updated."])
        self.assertEqual(self.shell.color, "green")
        self.assertEqual(self.shell.execute("color ultraviolet"), ["Usage: color cyan | green | white | amber"])
        self.assertEqual(self.shell.color, "green")

    def test_clear_and_blank_line_have_no_output(self) -> None:
        self.assertEqual(self.shell.execute("clear"), [])
        self.assertEqual(self.shell.execute("   "), [])

    def test_python_reboot_never_reboots_host(self) -> None:
        self.assertIn("cannot reboot the host", self.shell.execute("reboot")[0])

    def test_unknown_command_is_helpful(self) -> None:
        self.assertEqual(
            self.shell.execute("not-a-command"),
            ["Unknown command: not-a-command. Type 'help' for available commands."],
        )

    def test_command_line_mode(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ROOT / "python" / "britva.py"), "--command", "echo hello"],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.stdout, "hello\n")


class DiskImageTests(unittest.TestCase):
    def test_builds_a_valid_1440k_floppy_image(self) -> None:
        boot = bytearray(SECTOR_SIZE)
        boot[510:512] = b"\x55\xaa"
        kernel = b"KERNEL"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            boot_path = root / "boot.bin"
            kernel_path = root / "kernel.bin"
            image_path = root / "britva.img"
            boot_path.write_bytes(boot)
            kernel_path.write_bytes(kernel)

            make_image(boot_path, kernel_path, image_path)
            image = image_path.read_bytes()

        self.assertEqual(len(image), FLOPPY_SECTORS * SECTOR_SIZE)
        self.assertEqual(image[:SECTOR_SIZE], bytes(boot))
        self.assertEqual(image[SECTOR_SIZE : SECTOR_SIZE + len(kernel)], kernel)
        self.assertEqual(image[SECTOR_SIZE + len(kernel) :], bytes(len(image) - SECTOR_SIZE - len(kernel)))

    def test_rejects_boot_sector_without_signature(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            boot_path = root / "boot.bin"
            kernel_path = root / "kernel.bin"
            boot_path.write_bytes(bytes(SECTOR_SIZE))
            kernel_path.write_bytes(b"kernel")
            with self.assertRaisesRegex(ValueError, "signature"):
                make_image(boot_path, kernel_path, root / "out.img")

    def test_rejects_oversized_kernel(self) -> None:
        from mkimage import KERNEL_LOAD_SECTORS

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            boot = bytearray(SECTOR_SIZE)
            boot[510:512] = b"\x55\xaa"
            (root / "boot.bin").write_bytes(boot)
            (root / "kernel.bin").write_bytes(bytes(KERNEL_LOAD_SECTORS * SECTOR_SIZE + 1))
            with self.assertRaisesRegex(ValueError, "at most"):
                make_image(root / "boot.bin", root / "kernel.bin", root / "out.img")


if __name__ == "__main__":
    unittest.main()
