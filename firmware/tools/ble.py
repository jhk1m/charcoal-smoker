"""ble.py [cmd ...]  -- BLE debug client for SMOKER-CTRL (bleak 3.x, Windows).

Pair FIRST in Windows Settings > Bluetooth > Add device, within 60 s of a board reset, PIN = cfg::BLE_PASSKEY.
bleak on Windows can only do Just-Works pairing, which the firmware rejects (MITM required).

Subscribes to 000e (CSV telemetry) and 000d (console replies), prints both, sends each cmd on 000d with
1 s gaps, 'sleep N' is a pseudo-command. With no cmds it streams telemetry until Ctrl-C.
"""
import asyncio, sys, time
from bleak import BleakClient, BleakScanner

NAME = "SMOKER-CTRL"
BASE = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a%04x"
CH = {k: BASE % v for k, v in dict(pit=2, setpoint=3, duty=4, state=5, fault=6, info=0xA, warn=0xB,
                                   console=0xD, telem=0xE, timer=0xC).items()}

async def main(cmds):
    dev = await BleakScanner.find_device_by_name(NAME, timeout=15.0)
    if dev is None:
        sys.exit("not found: is the board powered and advertising?")
    t0 = time.time()
    tag = lambda s: print("%6.1f %s" % (time.time() - t0, s), flush=True)
    async with BleakClient(dev, timeout=20.0, winrt={"use_cached_services": False}) as c:
        tag("connected %s mtu=%d" % (dev.address, c.mtu_size))
        tag("info: " + (await c.read_gatt_char(CH["info"])).decode(errors="replace"))
        buf = {"console": b"", "telem": b""}
        def on(name):
            def cb(_, data):
                buf[name] += bytes(data)
                while b"\n" in buf[name]:
                    line, buf[name] = buf[name].split(b"\n", 1)
                    tag(("" if name == "telem" else "  ") + line.decode(errors="replace"))
            return cb
        await c.start_notify(CH["telem"], on("telem"))
        await c.start_notify(CH["console"], on("console"))
        if not cmds:
            while True:
                await asyncio.sleep(1)
        for cmd in cmds:
            if cmd.startswith("timer "):  # native uint16 write on 000c, then read-back
                import struct
                await c.write_gatt_char(CH["timer"], struct.pack("<H", int(cmd.split()[1])), response=True)
                tag("  timer read-back: %d min" % struct.unpack("<H", await c.read_gatt_char(CH["timer"]))[0])
                continue
            if cmd.startswith("sleep "):
                await asyncio.sleep(float(cmd.split()[1]))
                continue
            await c.write_gatt_char(CH["console"], cmd.encode(), response=True)
            await asyncio.sleep(1.0)
        await asyncio.sleep(1.0)

try:
    asyncio.run(main(sys.argv[1:]))
except KeyboardInterrupt:
    pass
