"""pair.py [COMx]  -- bond JASON-PC to SMOKER-CTRL with the static passkey (WinRT custom pairing, ProvidePin).
Optionally pulses RTS on COMx first to reset the board, so the 60 s pairing window is open.
Passkey is read from include/config.h, never typed on a command line."""
import asyncio, re, sys, time
from bleak import BleakScanner
from winrt.windows.devices.bluetooth import BluetoothLEDevice
from winrt.windows.devices.enumeration import DevicePairingKinds, DevicePairingProtectionLevel

CFG = __import__("os").path.join(__import__("os").path.dirname(__file__), "..", "include", "config.h")
PIN = re.search(r"BLE_PASSKEY\s*=\s*(\d+)", open(CFG).read()).group(1)

if len(sys.argv) > 1:
    import serial
    with serial.Serial() as s:
        s.port, s.baudrate = sys.argv[1], 115200
        s.dtr = False; s.rts = True; s.open(); time.sleep(0.1); s.rts = False; time.sleep(0.1)
    print("board reset, pairing window open 60 s")
    time.sleep(3)

async def main():
    dev = await BleakScanner.find_device_by_name("SMOKER-CTRL", timeout=15.0)
    if dev is None:
        sys.exit("not found")
    addr = int(dev.address.replace(":", ""), 16)
    ble = await BluetoothLEDevice.from_bluetooth_address_async(addr)
    pairing = ble.device_information.pairing
    print("paired already:", pairing.is_paired, "can pair:", pairing.can_pair)
    if pairing.is_paired:
        return
    custom = pairing.custom
    def on_req(sender, args):
        print("pairing kind:", args.pairing_kind)
        if args.pairing_kind == DevicePairingKinds.PROVIDE_PIN:
            args.accept_with_pin(PIN)
        else:
            args.accept()
    tok = custom.add_pairing_requested(on_req)
    try:
        res = await custom.pair_with_protection_level_async(
            DevicePairingKinds.PROVIDE_PIN | DevicePairingKinds.CONFIRM_ONLY,
            DevicePairingProtectionLevel.ENCRYPTION_AND_AUTHENTICATION)
        print("result:", res.status, "protection:", res.protection_level_used)
    finally:
        custom.remove_pairing_requested(tok)

asyncio.run(main())
