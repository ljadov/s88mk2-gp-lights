#!/usr/bin/env python3
"""Test du light guide S-series MK2 sans Gig Performer.
   pip install hidapi   puis   python test_lightguide.py
   Liste toutes les interfaces HID Native Instruments et essaie chacune.
"""
import time
import hid

VID = 0x17CC
KEYS = {0x1630: 88, 0x1620: 61, 0x1610: 49}

devs = hid.enumerate(VID, 0)
if not devs:
    raise SystemExit("Aucun peripherique HID Native Instruments (VID 17CC) vu par le systeme")
for d in devs:
    print(f"PID {d['product_id']:04X}  interface {d['interface_number']}  usage {d['usage_page']:04X}:{d['usage']:04X}  {d['product_string']}")

cands = sorted((d for d in devs if d['product_id'] in KEYS), key=lambda d: d['usage_page'] < 0xFF00)
for d in cands:
    keys = KEYS[d['product_id']]
    dev = hid.device()
    try:
        dev.open_path(d['path'])
        dev.write([0xA0, 0x00, 0x00])
    except (OSError, ValueError) as e:
        print(f"  echec sur interface {d['interface_number']} usage {d['usage_page']:04X} : {e}")
        continue
    print(f"OK sur interface {d['interface_number']} usage {d['usage_page']:04X} : balayage...")
    for i in range(keys):
        buf = [0] * keys
        for j in range(max(0, i - 8), i + 1):
            buf[j] = ((j % 16) + 1) * 4 + 2
        dev.write([0x81] + buf)
        time.sleep(0.02)
    time.sleep(1)
    dev.write([0x81] + [0] * keys)
    dev.close()
    break
else:
    print("Aucune interface n'a accepte l'ecriture")
