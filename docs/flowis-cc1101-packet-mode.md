# Flowis+ ECO — CC1101 hardware packet-mode test

This is an isolated receiver test for the newer Flowis+ ECO radio modules, including the reported `v24.C.11` generation.

It uses the **built-in ESPHome `cc1101` component** (available in ESPHome 2026.10.2) in hardware FIFO/packet mode. It does not use `rtl_433_ESP`, the external `rtl433:` component, or RadioLib's asynchronous pulse-capture path.

## Why this test exists

The Flowis+ report in [merbanan/rtl_433#2317](https://github.com/merbanan/rtl_433/issues/2317) describes a successful CC1101 packet-mode configuration for a newer `v24.C.11` module:

- 2-FSK, 100 kbaud
- frequency deviation 50 kHz
- RX filter 325 kHz
- IF frequency 203 kHz
- carrier about 868.044 MHz
- sync word D391, sync mode 30/32
- variable-length packets and hardware CRC

The current external `rtl_433_ESP` path receives pulse events, but the user's recent log contains no Flowis preamble/frame/CRC diagnostics. This test separates radio packet reception from the rtl_433 decoder path.

## Wiring

ESP32-WROOM-32 to CC1101:

| CC1101 | ESP32 |
|---|---:|
| CSN | GPIO5 |
| GDO0 | GPIO4 |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| VCC | 3.3 V |
| GND | GND |

GDO2 is not used by this native ESPHome packet-mode test.

## Install

1. Use ESPHome 2026.10.2 or newer.
2. Copy `example-flowis-cc1101-packet-mode.yaml` into ESPHome and provide the existing `wifi_ssid` and `wifi_password` secrets.
3. Compile and flash it to the same ESP32.
4. Open the ESPHome device logs and leave the receiver running near the meter.

**Do not add** the external `esphome_rtl433` `external_components:` block or an `rtl433:` section to this test. The built-in `cc1101:` component must be the only driver that initializes the radio.

## What the log means

Each accepted packet is logged as one line:

```text
[flowis_pkt] RX type=type-1 len=25 CRC=OK RSSI=-63.0 dBm FREQEST=+4000 Hz LQI=... DATA=[...]
```

- `type-1`: first data byte is `0x01`; the newer reports show 25 data bytes.
- `type-2`: first data byte is `0x02`; the newer reports show 19 data bytes.
- `len`: number of bytes delivered by ESPHome's `on_packet` callback. The variable-length prefix is consumed by the native driver before the callback.
- `CRC=OK`: the callback is only invoked for a packet that passes the CC1101 hardware CRC check because `crc_enable: true`.
- `RSSI`: received signal strength in dBm.
- `FREQEST`: signed CC1101 frequency-estimate value in Hz, as exposed by ESPHome 2026.10.2. The issue report measured about +44 kHz when tuned to 868.000 MHz; this test tunes to 868.040 MHz, so a similar module may show a residual offset around +4 kHz, not +44 kHz.
- `LQI`: link-quality indicator from the CC1101 status register.
- `DATA`: payload bytes in hexadecimal.

The expected packet payload sizes (25 and 19 bytes) come from the newer-module report in issue #2317. The test deliberately logs raw bytes only; it does not yet interpret meter ID, volume, monthly totals, timestamps, or alarms.

## If no packet appears

No `flowis_pkt` line means that the native component did not deliver a complete packet to the callback. With `crc_enable: true`, this can mean either no sync/packet was obtained or the CC1101 rejected the received CRC. It does not by itself distinguish no transmission, frequency/sync mismatch, packet truncation, and CRC failure.

The next controlled diagnostic pass is provided as [`example-flowis-cc1101-packet-mode-no-crc.yaml`](../example-flowis-cc1101-packet-mode-no-crc.yaml). It changes only `crc_enable` to `false` and marks every callback `CRC=NOT_CHECKED`. This lets sync-matched packets reach the callback even when their CRC would fail. Do not treat those packets as valid meter data until their CRC is checked independently. Do not change bitrate, deviation, bandwidth, frequency, and sync settings all at once.

### Interpreting the FIFO warning

ESPHome 2026.10.2 currently prints `RX FIFO overflow, flushing` for two different conditions: the actual RX FIFO overflow status bit is set **or** the reported RX byte count is zero. The log line does not distinguish these cases. The warning alone is therefore not proof that a packet exceeded the 64-byte FIFO. If it repeats without callbacks, the next code-level diagnostic should log both `RXBYTES.NUM_RXBYTES` and `RXBYTES.RXFIFO_OVERFLOW` separately.

## Important implementation notes

- This is a receiver test, not a production decoder.
- The current `flowis-868-fsk` branch remains unchanged.
- This branch adds the isolated native packet-mode YAML, this guide, and a README link.
- The current external `rtl433:` component cannot run at the same time as this native `cc1101:` instance on the same physical module.
