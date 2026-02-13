#!/usr/bin/env python3
"""
TENO Controller OTA Updater

A standalone script to update Teno Controller firmware via BLE.

Usage:
    python ota_updater.py                    # Scan and show device info
    python ota_updater.py --version          # Show firmware version
    python ota_updater.py --update firmware.bin  # Update with specific firmware
    python ota_updater.py --update --latest  # Update with latest firmware from build

Requirements:
    pip install bleak

Author: TENO Team
"""

import asyncio
import argparse
import struct
import sys
import os
import time
import hashlib
import re
import json
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple, List

try:
    from bleak import BleakClient, BleakScanner
    from bleak.backends.device import BLEDevice
except ImportError:
    print("Error: bleak library not installed")
    print("Install with: pip install bleak")
    sys.exit(1)


# =============================================================================
# OTA Protocol Constants
# =============================================================================

# BLE UUIDs
OTA_SERVICE_UUID = "fb1e4001-54ae-4a28-9f74-dfccb248601d"
OTA_CONTROL_CHAR_UUID = "fb1e4002-54ae-4a28-9f74-dfccb248601d"
OTA_DATA_CHAR_UUID = "fb1e4003-54ae-4a28-9f74-dfccb248601d"
OTA_STATUS_CHAR_UUID = "fb1e4004-54ae-4a28-9f74-dfccb248601d"
OTA_VERSION_CHAR_UUID = "fb1e4005-54ae-4a28-9f74-dfccb248601d"

# OTA Commands
OTA_CMD_START = 0x01
OTA_CMD_END = 0x02
OTA_CMD_ABORT = 0x03
OTA_CMD_REBOOT = 0x04

# OTA States
OTA_STATE_IDLE = 0x00
OTA_STATE_PREPARING = 0x01
OTA_STATE_RECEIVING = 0x02
OTA_STATE_VERIFYING = 0x03
OTA_STATE_COMPLETED = 0x04
OTA_STATE_ERROR = 0xFF

STATE_NAMES = {
    OTA_STATE_IDLE: "IDLE",
    OTA_STATE_PREPARING: "PREPARING",
    OTA_STATE_RECEIVING: "RECEIVING",
    OTA_STATE_VERIFYING: "VERIFYING",
    OTA_STATE_COMPLETED: "COMPLETED",
    OTA_STATE_ERROR: "ERROR"
}

# Error codes
ERROR_MESSAGES = {
    0x01: "Device not ready",
    0x02: "File size mismatch",
    0x03: "Write to flash failed",
    0x04: "Verification failed",
    0x05: "Not enough space",
    0x06: "Transfer timeout"
}

# Default paths
SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent
BUILD_DIR = PROJECT_DIR / ".pio" / "build" / "yd-esp32-s3"
DEFAULT_FIRMWARE = BUILD_DIR / "firmware.bin"
LOG_FILE = SCRIPT_DIR / "ota_history.log"

# Transfer settings
MAX_TRANSFER_RETRIES = 3
RETRY_DELAY = 2.0
REBOOT_WAIT_TIME = 8.0


# =============================================================================
# Version & Checksum Utilities
# =============================================================================

def parse_version(version_str: str) -> Tuple[List[int], str]:
    """
    Parse version string like '1.0.0.2-S3' into ([1, 0, 0, 2], 'S3')
    Returns (version_numbers, suffix)
    """
    if not version_str:
        return ([], "")

    # Split by dash to separate suffix
    parts = version_str.split('-', 1)
    version_part = parts[0]
    suffix = parts[1] if len(parts) > 1 else ""

    # Parse numeric parts
    numbers = []
    for p in version_part.split('.'):
        try:
            numbers.append(int(p))
        except ValueError:
            pass

    return (numbers, suffix)


def compare_versions(v1: str, v2: str) -> int:
    """
    Compare two version strings.
    Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
    """
    nums1, _ = parse_version(v1)
    nums2, _ = parse_version(v2)

    # Pad shorter list with zeros
    max_len = max(len(nums1), len(nums2))
    nums1.extend([0] * (max_len - len(nums1)))
    nums2.extend([0] * (max_len - len(nums2)))

    for n1, n2 in zip(nums1, nums2):
        if n1 < n2:
            return -1
        if n1 > n2:
            return 1

    return 0


def calculate_checksum(data: bytes) -> str:
    """Calculate MD5 checksum of data"""
    return hashlib.md5(data).hexdigest()


def get_firmware_version_from_binary(firmware_data: bytes) -> Optional[str]:
    """
    Try to extract version string from firmware binary.
    Looks for patterns like 'VERSION=x.x.x' or version strings.
    """
    # Look for common version patterns in the binary
    patterns = [
        rb'VERSION[=:]\s*([0-9]+\.[0-9]+\.[0-9]+[^\x00\s]*)',
        rb'FW_VERSION[=:]\s*([0-9]+\.[0-9]+\.[0-9]+[^\x00\s]*)',
        rb'(\d+\.\d+\.\d+\.\d+-[A-Z0-9]+)',  # e.g., 1.0.0.2-S3
    ]

    for pattern in patterns:
        match = re.search(pattern, firmware_data)
        if match:
            try:
                return match.group(1).decode('utf-8')
            except:
                pass

    return None


# =============================================================================
# Logging
# =============================================================================

def log_update(
    device_name: str,
    device_address: str,
    firmware_file: str,
    firmware_size: int,
    checksum: str,
    old_version: Optional[str],
    new_version: Optional[str],
    success: bool,
    duration: float,
    error: Optional[str] = None
):
    """Log OTA update to history file"""
    entry = {
        "timestamp": datetime.now().isoformat(),
        "device": {
            "name": device_name,
            "address": device_address
        },
        "firmware": {
            "file": firmware_file,
            "size": firmware_size,
            "md5": checksum
        },
        "version": {
            "old": old_version,
            "new": new_version
        },
        "result": {
            "success": success,
            "duration_seconds": round(duration, 1),
            "error": error
        }
    }

    try:
        with open(LOG_FILE, 'a') as f:
            f.write(json.dumps(entry, ensure_ascii=False) + '\n')
    except Exception as e:
        print_warning(f"Could not write log: {e}")


# =============================================================================
# Console Output Helpers
# =============================================================================

class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'


def print_header(text: str):
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.CYAN}  {text}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.ENDC}\n")


def print_success(text: str):
    print(f"{Colors.GREEN}✓ {text}{Colors.ENDC}")


def print_error(text: str):
    print(f"{Colors.RED}✗ {text}{Colors.ENDC}")


def print_warning(text: str):
    print(f"{Colors.YELLOW}⚠ {text}{Colors.ENDC}")


def print_info(text: str):
    print(f"{Colors.BLUE}ℹ {text}{Colors.ENDC}")


def print_progress(progress: float, speed: float, bytes_sent: int, total_bytes: int):
    bar_width = 40
    filled = int(bar_width * progress / 100)
    bar = '█' * filled + '░' * (bar_width - filled)

    remaining = total_bytes - bytes_sent
    eta = remaining / (speed * 1024) if speed > 0 else 0

    sys.stdout.write(f"\r  [{bar}] {progress:5.1f}% | {speed:.1f} KB/s | ETA: {eta:.0f}s  ")
    sys.stdout.flush()


async def print_countdown(seconds: float, message: str = "Waiting"):
    """Display animated countdown timer"""
    spinner = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
    start = time.time()
    idx = 0

    while True:
        elapsed = time.time() - start
        remaining = seconds - elapsed

        if remaining <= 0:
            sys.stdout.write(f"\r{Colors.BLUE}✓ {message} complete{' ' * 20}{Colors.ENDC}\n")
            sys.stdout.flush()
            break

        spin = spinner[idx % len(spinner)]
        bar_width = 20
        filled = int(bar_width * elapsed / seconds)
        bar = '█' * filled + '░' * (bar_width - filled)

        sys.stdout.write(f"\r{Colors.BLUE}{spin} {message} [{bar}] {remaining:.0f}s {Colors.ENDC}")
        sys.stdout.flush()

        idx += 1
        await asyncio.sleep(0.1)


# =============================================================================
# BLE Scanner
# =============================================================================

async def scan_teno_devices(timeout: float = 5.0) -> list[BLEDevice]:
    """Scan for TENO Controller devices (excludes TENO-Core)"""
    print_info(f"Scanning for TENO Controller devices ({timeout}s)...")

    devices = await BleakScanner.discover(timeout=timeout)
    # Filter: starts with "TENO" but exclude "TENO-Core" (Linux bridge, not a controller)
    teno_devices = [
        d for d in devices
        if d.name and d.name.startswith("TENO") and d.name != "TENO-Core"
    ]

    return teno_devices


async def select_device(devices: list[BLEDevice]) -> Optional[BLEDevice]:
    """Let user select a device from list"""
    if not devices:
        print_error("No TENO devices found")
        return None

    if len(devices) == 1:
        print_success(f"Found device: {devices[0].name} ({devices[0].address})")
        return devices[0]

    print(f"\nFound {len(devices)} TENO devices:\n")
    for i, d in enumerate(devices):
        print(f"  [{i+1}] {d.name} ({d.address})")

    while True:
        try:
            choice = input(f"\nSelect device [1-{len(devices)}]: ")
            idx = int(choice) - 1
            if 0 <= idx < len(devices):
                return devices[idx]
        except (ValueError, KeyboardInterrupt):
            return None
        print_warning("Invalid selection")


# =============================================================================
# OTA Updater Class
# =============================================================================

class OTAUpdater:
    def __init__(self, device: BLEDevice):
        self.device = device
        self.client: Optional[BleakClient] = None
        self.current_state = OTA_STATE_IDLE
        self.last_error = 0

    async def connect(self) -> bool:
        """Connect to device"""
        print_info(f"Connecting to {self.device.name}...")

        try:
            self.client = BleakClient(self.device.address)
            await self.client.connect()

            if self.client.is_connected:
                print_success("Connected!")
                return True
            else:
                print_error("Connection failed")
                return False

        except Exception as e:
            print_error(f"Connection error: {e}")
            return False

    async def disconnect(self):
        """Disconnect from device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print_info("Disconnected")

    async def get_version(self) -> Optional[str]:
        """Read firmware version"""
        if not self.client or not self.client.is_connected:
            return None

        try:
            # Try OTA version characteristic first
            data = await self.client.read_gatt_char(OTA_VERSION_CHAR_UUID)
            version = data.decode('utf-8').strip('\x00')
            return version
        except Exception as e:
            print_warning(f"Could not read version: {e}")
            return None

    async def read_status(self) -> Tuple[int, int]:
        """Read OTA status"""
        try:
            data = await self.client.read_gatt_char(OTA_STATUS_CHAR_UUID)
            state = data[0] if len(data) > 0 else OTA_STATE_IDLE
            error = data[1] if len(data) > 1 else 0
            return state, error
        except Exception:
            return OTA_STATE_IDLE, 0

    async def write_control(self, data: bytes):
        """Write to control characteristic"""
        await self.client.write_gatt_char(OTA_CONTROL_CHAR_UUID, data, response=True)

    async def write_data(self, data: bytes):
        """Write to data characteristic (no response for speed)"""
        await self.client.write_gatt_char(OTA_DATA_CHAR_UUID, data, response=False)

    async def _transfer_with_retry(
        self,
        firmware_data: bytes,
        firmware_size: int,
        chunk_size: int,
        start_time: float
    ) -> bool:
        """Transfer firmware data with retry on failure"""
        total_packets = (firmware_size + chunk_size - 1) // chunk_size

        for attempt in range(MAX_TRANSFER_RETRIES):
            try:
                seq = 0
                bytes_sent = 0

                for offset in range(0, firmware_size, chunk_size):
                    chunk = firmware_data[offset:offset + chunk_size]
                    packet = struct.pack('<H', seq) + chunk

                    await self.write_data(packet)

                    seq += 1
                    bytes_sent += len(chunk)

                    # Progress
                    progress = (bytes_sent / firmware_size) * 100
                    elapsed = time.time() - start_time
                    speed = bytes_sent / elapsed / 1024 if elapsed > 0 else 0

                    print_progress(progress, speed, bytes_sent, firmware_size)

                    # Small delay every N packets for stability
                    if seq % 20 == 0:
                        await asyncio.sleep(0.01)

                print()  # New line after progress bar
                return True

            except Exception as e:
                print()  # New line after progress bar
                if attempt < MAX_TRANSFER_RETRIES - 1:
                    print_warning(f"Transfer error at packet {seq}: {e}")
                    print_info(f"Retrying ({attempt + 2}/{MAX_TRANSFER_RETRIES})...")
                    await asyncio.sleep(RETRY_DELAY)

                    # Abort and restart
                    try:
                        await self.write_control(bytes([OTA_CMD_ABORT]))
                        await asyncio.sleep(1.0)
                        size_bytes = struct.pack('<I', firmware_size)
                        await self.write_control(bytes([OTA_CMD_START]) + size_bytes)
                        await asyncio.sleep(0.5)
                    except:
                        pass
                else:
                    print_error(f"Transfer failed after {MAX_TRANSFER_RETRIES} attempts: {e}")
                    return False

        return False

    async def verify_after_reboot(self, max_retries: int = 3) -> Optional[str]:
        """Reconnect after reboot and verify new firmware version"""
        for attempt in range(max_retries):
            try:
                # Disconnect if still connected
                if self.client and self.client.is_connected:
                    await self.client.disconnect()

                # Reconnect
                self.client = BleakClient(self.device.address)
                await self.client.connect()

                if self.client.is_connected:
                    version = await self.get_version()
                    await self.client.disconnect()
                    return version

            except Exception as e:
                if attempt < max_retries - 1:
                    print_info(f"Retry {attempt + 2}/{max_retries}...")
                    await asyncio.sleep(2.0)

        return None

    async def update_firmware(
        self,
        firmware_path: Path,
        current_version: Optional[str] = None,
        auto_confirm: bool = False
    ) -> Tuple[bool, Optional[str], Optional[str]]:
        """
        Perform OTA update with checksum verification and retry support.
        Returns: (success, new_version, error_message)
        """
        if not self.client or not self.client.is_connected:
            print_error("Not connected")
            return (False, None, "Not connected")

        # Read firmware file
        if not firmware_path.exists():
            print_error(f"Firmware file not found: {firmware_path}")
            return (False, None, "File not found")

        firmware_data = firmware_path.read_bytes()
        firmware_size = len(firmware_data)
        checksum = calculate_checksum(firmware_data)

        # Try to get version from binary
        binary_version = get_firmware_version_from_binary(firmware_data)

        print_info(f"Firmware: {firmware_path.name}")
        print_info(f"Size: {firmware_size:,} bytes ({firmware_size/1024:.1f} KB)")
        print_info(f"MD5: {checksum}")

        if binary_version:
            print_info(f"Firmware version: {binary_version}")

        # Version comparison
        if current_version and binary_version:
            cmp_result = compare_versions(current_version, binary_version)
            if cmp_result == 0:
                print_warning(f"Firmware version is the same as current ({current_version})")
                if not auto_confirm:
                    confirm = input(f"{Colors.YELLOW}Continue anyway? [y/N]: {Colors.ENDC}")
                    if confirm.lower() != 'y':
                        print_info("Update cancelled")
                        return (False, None, "Cancelled by user")
            elif cmp_result > 0:
                print_warning(f"Firmware version ({binary_version}) is older than current ({current_version})")
                if not auto_confirm:
                    confirm = input(f"{Colors.YELLOW}Downgrade? [y/N]: {Colors.ENDC}")
                    if confirm.lower() != 'y':
                        print_info("Update cancelled")
                        return (False, None, "Cancelled by user")
            else:
                print_success(f"Upgrade: {current_version} → {binary_version}")

        # Confirm (Enter = yes)
        if not auto_confirm:
            confirm = input(f"\n{Colors.YELLOW}Start OTA update? [Y/n]: {Colors.ENDC}")
            if confirm.lower() == 'n':
                print_info("Update cancelled")
                return (False, None, "Cancelled by user")
        else:
            print_info("Auto-confirm enabled, starting update...")

        print()
        start_time = time.time()

        try:
            # 1. Send OTA_START
            print_info("Starting OTA update...")
            size_bytes = struct.pack('<I', firmware_size)
            await self.write_control(bytes([OTA_CMD_START]) + size_bytes)

            await asyncio.sleep(0.5)
            state, error = await self.read_status()

            if state == OTA_STATE_ERROR:
                error_msg = ERROR_MESSAGES.get(error, f"Unknown error: 0x{error:02X}")
                print_error(f"Device error: {error_msg}")
                return False

            # 2. Transfer data with retry support
            print_info("Transferring firmware...")

            chunk_size = 500  # Safe default
            success = await self._transfer_with_retry(
                firmware_data, firmware_size, chunk_size, start_time
            )

            if not success:
                return False

            elapsed = time.time() - start_time
            print_success(f"Transfer complete: {firmware_size:,} bytes in {elapsed:.1f}s ({firmware_size/elapsed/1024:.1f} KB/s)")

            # 3. Send OTA_END
            print_info("Verifying firmware...")
            await self.write_control(bytes([OTA_CMD_END]))

            await asyncio.sleep(2.0)

            # 4. Check status
            state, error = await self.read_status()

            if state == OTA_STATE_COMPLETED:
                print_success("Firmware verified successfully!")

                # 5. Reboot
                print_info("Rebooting device...")
                await asyncio.sleep(0.5)

                try:
                    await self.write_control(bytes([OTA_CMD_REBOOT]))
                except Exception:
                    pass  # Device may disconnect immediately

                total_time = time.time() - start_time
                print()
                print_header("OTA UPDATE COMPLETE")
                print_success(f"Total time: {total_time:.1f}s")

                # 6. Wait with countdown animation
                await print_countdown(REBOOT_WAIT_TIME, "Device rebooting")

                print_info("Reconnecting to verify new firmware...")
                new_version = await self.verify_after_reboot()

                if new_version:
                    print()
                    print_success(f"New firmware version: {new_version}")
                else:
                    print_warning("Could not verify new firmware (device may still be restarting)")

                # Log the update
                log_update(
                    device_name=self.device.name,
                    device_address=self.device.address,
                    firmware_file=firmware_path.name,
                    firmware_size=firmware_size,
                    checksum=checksum,
                    old_version=current_version,
                    new_version=new_version,
                    success=True,
                    duration=total_time
                )

                return (True, new_version, None)

            elif state == OTA_STATE_ERROR:
                error_msg = ERROR_MESSAGES.get(error, f"Unknown error: 0x{error:02X}")
                print_error(f"Verification failed: {error_msg}")
                return (False, None, error_msg)
            else:
                error_msg = f"Unexpected state: {STATE_NAMES.get(state, f'0x{state:02X}')}"
                print_warning(error_msg)
                return (False, None, error_msg)

        except Exception as e:
            print_error(f"OTA error: {e}")

            # Log failure
            log_update(
                device_name=self.device.name,
                device_address=self.device.address,
                firmware_file=firmware_path.name,
                firmware_size=firmware_size,
                checksum=checksum,
                old_version=current_version,
                new_version=None,
                success=False,
                duration=time.time() - start_time,
                error=str(e)
            )

            # Try to abort
            try:
                await self.write_control(bytes([OTA_CMD_ABORT]))
            except Exception:
                pass

            return (False, None, str(e))


# =============================================================================
# Main Functions
# =============================================================================

async def show_device_info(device: BLEDevice):
    """Connect and show device info"""
    updater = OTAUpdater(device)

    if not await updater.connect():
        return

    try:
        version = await updater.get_version()
        state, error = await updater.read_status()

        print()
        print_header("TENO Controller Info")
        print(f"  Device Name:      {device.name}")
        print(f"  Address:          {device.address}")
        print(f"  Firmware Version: {version or 'Unknown'}")
        print(f"  OTA State:        {STATE_NAMES.get(state, f'0x{state:02X}')}")
        print()

    finally:
        await updater.disconnect()


async def update_device(device: BLEDevice, firmware_path: Path, auto_confirm: bool = False):
    """Update device firmware"""
    updater = OTAUpdater(device)

    if not await updater.connect():
        return False

    try:
        # Get current version for comparison
        current_version = await updater.get_version()
        if current_version:
            print_info(f"Current firmware: {current_version}")

        # Perform update with version comparison
        success, new_version, error = await updater.update_firmware(
            firmware_path, current_version, auto_confirm
        )
        return success

    finally:
        try:
            await updater.disconnect()
        except Exception:
            pass


async def update_multiple_devices(
    devices: List[BLEDevice],
    firmware_path: Path,
    auto_confirm: bool = False
):
    """Update multiple devices sequentially"""
    results = []

    print_header(f"Batch Update: {len(devices)} devices")

    for i, device in enumerate(devices):
        print(f"\n{'='*60}")
        print(f"  [{i+1}/{len(devices)}] {device.name} ({device.address})")
        print(f"{'='*60}\n")

        success = await update_device(device, firmware_path, auto_confirm)
        results.append((device, success))

        if i < len(devices) - 1:
            print_info("Waiting before next device...")
            await asyncio.sleep(2.0)

    # Summary
    print()
    print_header("Batch Update Summary")
    success_count = sum(1 for _, s in results if s)
    fail_count = len(results) - success_count

    for device, success in results:
        status = f"{Colors.GREEN}✓ SUCCESS{Colors.ENDC}" if success else f"{Colors.RED}✗ FAILED{Colors.ENDC}"
        print(f"  {device.name}: {status}")

    print()
    print(f"  Total: {success_count} succeeded, {fail_count} failed")

    return all(s for _, s in results)


async def main():
    parser = argparse.ArgumentParser(
        description="TENO Controller OTA Updater",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              Scan and show device info
  %(prog)s --version                    Show firmware version only
  %(prog)s --update firmware.bin        Update with specific firmware
  %(prog)s --update --latest            Update with latest build
  %(prog)s --update --latest --yes      Auto-confirm update (no prompts)
  %(prog)s --update --latest --all      Update all found devices
  %(prog)s --address XX:XX:XX:XX:XX:XX  Connect to specific device
  %(prog)s --history                    Show update history
        """
    )

    parser.add_argument('--version', '-v', action='store_true',
                        help='Show firmware version')
    parser.add_argument('--update', '-u', nargs='?', const='__latest__', metavar='FILE',
                        help='Update firmware (use --latest for latest build)')
    parser.add_argument('--latest', '-l', action='store_true',
                        help='Use latest firmware from build directory')
    parser.add_argument('--yes', '-y', action='store_true',
                        help='Auto-confirm all prompts (non-interactive mode)')
    parser.add_argument('--all', action='store_true',
                        help='Update all found devices (batch mode)')
    parser.add_argument('--address', '-a', metavar='ADDR',
                        help='Device BLE address (skip scanning)')
    parser.add_argument('--scan-timeout', '-t', type=float, default=5.0,
                        help='Scan timeout in seconds (default: 5)')
    parser.add_argument('--history', action='store_true',
                        help='Show OTA update history')

    args = parser.parse_args()

    print_header("TENO Controller OTA Updater")

    # Show history if requested
    if args.history:
        await show_history()
        return 0

    # Find device(s)
    devices = []

    if args.address:
        # Create device from address
        print_info(f"Using address: {args.address}")
        all_devices = await BleakScanner.discover(timeout=args.scan_timeout)
        device = next((d for d in all_devices if d.address == args.address), None)
        if not device:
            print_error(f"Device not found: {args.address}")
            return 1
        devices = [device]
    else:
        # Scan for devices
        devices = await scan_teno_devices(timeout=args.scan_timeout)

        if not devices:
            print_error("No TENO devices found")
            return 1

        # Batch mode or single device
        if args.all:
            print_success(f"Found {len(devices)} devices for batch update")
        else:
            device = await select_device(devices)
            if not device:
                return 1
            devices = [device]

    # Determine action
    if args.update:
        # Update firmware
        if args.latest or args.update == '__latest__':
            firmware_path = DEFAULT_FIRMWARE
            if not firmware_path.exists():
                print_error(f"Latest firmware not found: {firmware_path}")
                print_info("Build firmware first: pio run")
                return 1
        else:
            firmware_path = Path(args.update)

        print()

        # Batch or single update
        if len(devices) > 1:
            success = await update_multiple_devices(devices, firmware_path, args.yes)
        else:
            success = await update_device(devices[0], firmware_path, args.yes)

        return 0 if success else 1

    else:
        # Show device info (single device only)
        await show_device_info(devices[0])
        return 0


async def show_history():
    """Show OTA update history from log file"""
    if not LOG_FILE.exists():
        print_info("No update history found")
        return

    print_header("OTA Update History")

    try:
        with open(LOG_FILE, 'r') as f:
            lines = f.readlines()

        if not lines:
            print_info("No updates recorded")
            return

        # Show last 10 entries
        recent = lines[-10:] if len(lines) > 10 else lines

        for line in recent:
            try:
                entry = json.loads(line.strip())
                timestamp = entry.get('timestamp', '')[:19].replace('T', ' ')
                device = entry.get('device', {}).get('name', 'Unknown')
                old_ver = entry.get('version', {}).get('old', '?')
                new_ver = entry.get('version', {}).get('new', '?')
                success = entry.get('result', {}).get('success', False)
                duration = entry.get('result', {}).get('duration_seconds', 0)

                status = f"{Colors.GREEN}✓{Colors.ENDC}" if success else f"{Colors.RED}✗{Colors.ENDC}"
                print(f"  {status} {timestamp} | {device} | {old_ver} → {new_ver} | {duration}s")
            except:
                pass

        print()
        print_info(f"Log file: {LOG_FILE}")
        print_info(f"Total entries: {len(lines)}")

    except Exception as e:
        print_error(f"Could not read history: {e}")


if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n")
        print_info("Cancelled by user")
        sys.exit(130)
