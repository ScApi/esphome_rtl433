# Flowis+ ECO — CC1101 hardware packet-mode test

This is an isolated receiver test for the newer Flowis+ ECO radio modules, including the reported `v24.C.11` generation.

It uses ESPHome's `cc1101:` component in hardware FIFO/packet mode. The diagnostic variant overrides the bundled ESPHome CC1101 component with the source in this repository, based on upstream **ESPHome 2026.10.2**. It does not use `rtl_433_ESP`, the separate `rtl433:` external component, or RadioLib's asynchronous pulse-capture path.

## Why this test exists

The Flowis+ report in [merbanan/rtl_433#2317](https://github.com/merbanan/rtl_433/issues/2317) describes a successful CC1101 packet-mode configuration for a newer `v24.C.11` module:

- 2-FSK, 100 kbaud
- frequency deviation 50 kHz
- RX filter 325 kHz
- IF frequency 203 kHz
- carrier about 868.044 MHz
- sync word D391, sync mode 30/32
- variable-length packets and hardware CRC

The native packet-mode test separates radio packet reception from the rtl_433 decoder path.

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

## Install from the Home Assistant ESPHome dashboard

1. Use ESPHome **2026.10.2 or newer**.
2. In the ESPHome device YAML editor, either use [the diagnostic example](../example-flowis-cc1101-packet-mode-diagnostic.yaml) as the starting point or copy its `external_components:` block into your existing Flowis YAML.
3. Keep your existing Wi-Fi secrets and the current radio settings. Do not change `crc_enable: true` for this diagnostic run.
4. Save, validate, and compile from the ESPHome dashboard. Then install the compiled firmware to the ESP32.
5. Open the device logs and leave the receiver running near the meter.

The required override block is:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ScApi/esphome_rtl433
      ref: experiment/flowis-cc1101-packet-diagnostics
      path: ha-external-components
    components: [cc1101]
    refresh: 0s  # Force GitHub check on every validation during diagnostics
```

Place it at the top level of the YAML. If your file already contains `external_components:`, append this source to its list; do not create a second key.

This repository folder deliberately contains a same-named `cc1101` component, which ESPHome loads as an override of its bundled component. You do **not** need to access the add-on container, use a Windows shell, edit ESPHome's installed Python package, or copy files manually.

**Do not add** the separate `rtl433:` component or the old `esphome_rtl433/components` source to this packet-mode test. The override remains the only component that initializes the CC1101 on SPI. It is an external override for `cc1101:`, not a second radio driver.

## What the log means

The existing `on_packet` lambda continues to log each accepted packet:

```text
[flowis_pkt] RX type=type-1 len=25 CRC=OK RSSI=-63.0 dBm FREQEST=+4000 Hz LQI=... DATA=[...]
```

- `type-1`: first data byte is `0x01`; the newer reports show 25 data bytes.
- `type-2`: first data byte is `0x02`; the newer reports show 19 data bytes.
- `len`: bytes delivered by ESPHome's `on_packet` callback. The variable-length prefix is consumed by the driver.
- `CRC=OK`: the callback passed the component's hardware CRC acceptance rule.
- `RSSI`, `FREQEST`, `LQI`, and `DATA`: radio status and raw payload bytes.

The driver now also emits these diagnostic messages:

| Log tag | Meaning |
|---|---|
| `FLOWIS_DIAG FIFO_OVERFLOW` | RXBYTES reports the CC1101 RX FIFO overflow status bit. |
| `FLOWIS_DIAG FIFO_EMPTY` | GDO0 was high but RXBYTES reported zero bytes and no overflow bit. |
| `FLOWIS_DIAG INVALID_PACKET` | The received variable-length prefix or FIFO byte count failed the driver's length validation. |
| `FLOWIS_DIAG CRC_REJECT` | The driver read a structurally accepted packet, but the CC1101 CRC status bit was not set while CRC was enabled. |
| `FLOWIS_DIAG PACKET_ACCEPT` | The packet passed the component's current CRC acceptance rule and is passed to the existing callback. |

The diagnostic messages include raw RXBYTES and, where relevant, the MARCSTATE. The current code intentionally preserves ESPHome's original RX/IDLE/FIFO-flush state transitions. It separates the different reasons that the original generic warning conflated; it does not yet redesign packet acquisition.

## If no packet appears

No `flowis_pkt` line means no packet reached the existing callback. The diagnostic tags above should now indicate whether the driver saw an empty FIFO, a hardware overflow, an invalid length/count, or a CRC rejection.

The controlled no-CRC variant for this same external-component diagnostic build is [`example-flowis-cc1101-packet-mode-diagnostic-no-crc.yaml`](../example-flowis-cc1101-packet-mode-diagnostic-no-crc.yaml). It changes only `crc_enable` to `false`, retains the diagnostic component override, and labels callback packets as `CRC=NOT_CHECKED`. Use it only as a separate experiment after collecting the CRC-enabled logs. Do not treat those packets as valid meter data until their CRC is independently checked. Do not change bitrate, deviation, bandwidth, frequency, and sync settings all at once.

## Important implementation notes

- This is a receiver diagnostic, not a production decoder.
- The upstream CC1101 component sources are pinned to ESPHome 2026.10.2; the diagnostic modifications are limited to `CC1101Component::loop()`.
- The code has been placed in the GitHub branch, but it has **not yet been compiled in your Home Assistant ESPHome environment or tested on the ESP32/CC1101 hardware**.
- The current external `rtl433:` component cannot run at the same time as this native `cc1101:` instance on the same physical module.
