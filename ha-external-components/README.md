# Flowis CC1101 packet diagnostics for Home Assistant ESPHome

This folder is an external-component override of the bundled ESPHome `cc1101`
component. It is based on upstream ESPHome **2026.10.2**. The Python schema,
header files and register definitions are copied from that exact tag. The
`cc1101.cpp` override adds diagnostic logging, separates FIFO/CRC conditions,
and lowers the RX FIFO interrupt threshold to 32 bytes so overlong variable-
length frames can be identified earlier, before they fill the 64-byte FIFO.

## Add to the existing Flowis YAML

Add this block at the root level. If the YAML already contains an
`external_components:` key, append this source to its existing list instead
of creating a duplicate key.

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

The component name remains `cc1101:` and the existing packet-mode YAML does
not need to be otherwise changed. Keep `crc_enable: true` for this diagnostic
run. Do not load the separate `rtl433:` external component at the same time
as the native `cc1101:` packet-mode driver.

## Expected diagnostic tags

- `FLOWIS_DIAG FIFO_OVERFLOW`: CC1101 reported its RX FIFO overflow flag.
- `FLOWIS_DIAG FIFO_EMPTY`: GDO0 was high but RXBYTES contained no bytes.
- `FLOWIS_DIAG INVALID_PACKET`: the first length byte or FIFO byte count did
  not match variable-length packet expectations. The lower threshold is intended
  to expose this earlier for frames whose length byte is greater than 64.
- `FLOWIS_DIAG CRC_REJECT`: a packet was read, but the CC1101 CRC status bit
  was not set while CRC checking was enabled.
- `FLOWIS_DIAG PACKET_ACCEPT`: the packet passed the component's current
  CRC acceptance rule and is being sent to the existing `on_packet` callback.

These logs are diagnostic observations, not proof that a particular radio
parameter is incorrect. The firmware must still be compiled and tested on the
target ESP32/CC1101 hardware.
