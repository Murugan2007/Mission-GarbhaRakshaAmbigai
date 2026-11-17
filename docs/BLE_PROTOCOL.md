# BLE protocol v1

Compact summaries instead of raw signals: lower power, more privacy.

| Item | Value |
| --- | --- |
| Link | Bluetooth Low Energy 5, LE Secure Connections pairing, encrypted |
| Service UUID | `6e1a0001-b5a3-f393-e0a9-e50e24dcca9e` |
| Summary characteristic (notify) | `6e1a0002-b5a3-f393-e0a9-e50e24dcca9e` |
| Rate | About one packet every 30 s; immediately on a change of state |

## Summary packet (10 bytes, little-endian)

| Bytes | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0-3 | u32 | `device_ms` | Milliseconds since the wearable started |
| 4-5 | u16 | `median_pat_x10` | Rolling median pulse arrival time in tenths of a millisecond |
| 6 | u8 | `good_beats` | Beats in the median (0-15) |
| 7 | u8 | `flags` | bit0 chest electrodes off, bit1 no finger on sensor, bit2 motion |
| 8-9 | u16 | `rr_ms` | Latest beat-to-beat interval in ms |

The phone stamps each packet with its own clock on arrival. A packet is "usable" when `good_beats >= 8` and `flags == 0`.

Example: `E8 03 00 00 A7 08 0F 00 34 03` = 1000 ms uptime, 221.5 ms PAT, 15 good beats, no flags, RR 820 ms. The Flutter parser (`BeatSummary.parse`) is unit-tested against this packet.

## Optional raw stream

A second notify characteristic (`6e1a0003-...`) is reserved for a raw ECG and pulse stream, used only during consented data collection.
