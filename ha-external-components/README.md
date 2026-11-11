# Flowis+ ECO CC1101 packet-mode component override

This directory overrides ESPHome's bundled `cc1101:` component. The source is based on upstream
ESPHome **2026.10.2**. It retains the ESPHome component API and SPI configuration, with the
packet-mode receive handling used by the Flowis+ ECO example.

The receiver configures GDO0 for the packet-end/sync signal and reads the RX FIFO after the
falling edge. Diagnostic logs distinguish FIFO overflow, empty FIFO, invalid packet length,
CRC rejection and CRC-accepted packets.

## Use

The supported, maintained configuration is:

- [Complete ESPHome example with Flowis Type-1/Type-2 decoding](../example-flowis-cc1101-homeassistant.yaml)
- [Compatibility notes, exact radio settings, frame layouts and Home Assistant setup](../docs/flowis-cc1101-packet-mode.md)

Add the source to the top level of the device YAML (or append it to an existing
external_components list):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ScApi/esphome_rtl433
      ref: experiment/flowis-cc1101-packet-diagnostics
      path: ha-external-components
    components: [cc1101]
    refresh: 0s
```

Use ESPHome 2026.10.2, the version against which this override was prepared and tested.
The complete example retains the verified 868.00 MHz tuning from the current installation,
hardware CRC checking and the Flowis packet decoder.

**Do not load** the separate `rtl433:` / rtl_433_ESP component at the same time. Both drivers
would attempt to control the same physical CC1101 over SPI. This packet-mode override is the
only CC1101 driver in this configuration.
