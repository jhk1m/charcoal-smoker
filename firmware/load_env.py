# PlatformIO pre-script: reads firmware/.env (git-ignored) and passes each KEY=VALUE as -DKEY=VALUE.
# Holds the BLE passkey so it never lands in the repo. Copy .env.example to .env and set a value.
import os
Import("env")

path = os.path.join(env["PROJECT_DIR"], ".env")
if not os.path.isfile(path):
    raise SystemExit("firmware/.env missing: copy .env.example to .env and set SMOKER_BLE_PASSKEY (6 digits)")
with open(path, encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        env.Append(CPPDEFINES=[(k.strip(), v.strip())])
