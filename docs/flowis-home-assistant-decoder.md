# Flowis+ ECO4 decoder → ESPHome → Home Assistant

## Receiver state

The decoder example is designed for the successful ESP32-WROOM-32 + CC1101 packet-mode setup:

- Frequency: **868.00 MHz** (the user's successful test setting)
- 2-FSK, 100 kbaud, 50 kHz deviation
- RX filter bandwidth 325 kHz, IF 203 kHz
- Sync D391, mode 30/32
- Variable-length packets, hardware CRC enabled
- ESPHome 2026.10.2+
- CC1101 is owned only by the native `cc1101:` component override in this repository

Use this file as the reference configuration:

[`example-flowis-cc1101-home-assistant.yaml`](../example-flowis-cc1101-home-assistant.yaml)

It is a complete example. **Do not replace the user's entire working `flowis.yaml` with it.** Merge only the sections described below.

## 1. Update the existing ESPHome YAML

The existing file should retain its Wi-Fi, API, OTA, SPI, ESP32 and external component settings. Do not add a second CC1101 component, a second `external_components:` root key, or the separate `rtl433:` receiver. The native CC1101 packet component must remain the only driver of the physical radio.

Keep the branch override:

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

In the existing `cc1101:` block, keep the working radio parameters and `frequency: 868.00MHz`, `packet_mode: true`, `packet_length: 0`, `crc_enable: true`. Replace only its `on_packet:` lambda with the one in the example file.

Copy the example's `sensor:` and `text_sensor:` entries into the existing root-level sections. If `sensor:` or `text_sensor:` already exists, append these list entries to it; do not create duplicate YAML keys.

The decoder is deliberately filtered to meter ID **241047300** so that transmissions from other nearby Flowis meters are ignored. The old assumed ID 241047064 is not used.

## 2. Sensors created by ESPHome

Entity IDs are normally generated from the names below. Confirm the exact IDs in Home Assistant under **Settings → Devices & services → ESPHome → Flowis device → Entities** or **Developer tools → States**.

| Entity name | Meaning |
|---|---|
| `sensor.flowis_stan_calkowity` | Current cumulative meter register from Type 1; m³, water, total_increasing |
| `sensor.flowis_zuzycie_biezacy_miesiac` | Current month usage from Type 2; m³, water, total_increasing |
| `sensor.flowis_biezacy_miesiac_kontrola` | Cross-check: Type 1 current register minus previous-month-end register |
| `sensor.flowis_stan_na_koniec_poprzedniego_miesiaca` | Previous-month-end cumulative register snapshot |
| `sensor.flowis_zuzycie_poprzedni_miesiac` | Previous completed month's consumption |
| `sensor.flowis_zuzycie_2_miesiace_temu` … `sensor.flowis_zuzycie_6_miesiecy_temu` | Older monthly consumption values |
| `sensor.flowis_rssi`, `sensor.flowis_lqi` | Radio reception diagnostics |
| `sensor.flowis_backflow_raw`, `sensor.flowis_alarm_raw` | Raw status bytes; meaning not yet conclusively interpreted |
| `text_sensor.flowis_numer_licznika` | Meter serial (241047300) |
| `text_sensor.flowis_czas_licznika` | Timestamp encoded by the meter |

The current total and current month sensors use `state_class: total_increasing`. The historical month snapshots are display-only; they intentionally do not claim to be live measurements or create misleading long-term statistics. The previous-month-end register is a cumulative snapshot, not monthly consumption.

## 3. Preserve the last valid Home Assistant reading

The user's existing template sensor named `Licznik wody (ostatni odczyt)` can stay. Change only the source entity in its Jinja template to the new ESPHome total sensor. Once the actual entity ID has been confirmed in Home Assistant, the state template should be:

```jinja
{% set current = states('sensor.flowis_stan_calkowity') %}
{% if is_number(current) and (current | float) > 0 %}
  {{ current | float }}
{% elif this.state is defined and is_number(this.state) %}
  {{ this.state | float }}
{% else %}
  0
{% endif %}
```

Keep the existing metadata unchanged:

- `unique_id: licznik_wody_ostatni_odczyt`
- `unit_of_measurement: "m³"`
- `device_class: water`
- `state_class: total_increasing`

If the new ESPHome entity got a slightly different entity ID, replace `sensor.flowis_stan_calkowity` with the exact ID shown in Home Assistant. Do not use the old `sensor.flowis_241047064_water_usage` source.

This template keeps the previous value when the new source is unavailable, unknown, non-numeric, or zero. The ESPHome decoder also avoids publishing zero as a replacement for a missing/invalid Type-1 reading and ignores a Type-2 `0xFFFF` monthly slot.

## 4. Meaning of the fields in the first received sample

For the sample packet received on 2026-11-11:

- Type 1 cumulative register: **40.059 m³**
- Type 1 previous-month-end cumulative register: **36.841 m³**
- Difference between those two Type 1 values: **3.218 m³**
- Type 2 current month: **3.218 m³**
- Type 2 previous month: **11.230 m³**
- Type 2 two months ago: **10.131 m³**
- Type 2 three months ago: **10.617 m³**
- Type 2 four months ago: **4.612 m³**
- Type 2 five months ago: **0.001 m³**
- Type 2 six months ago: raw `0xFFFF`; not published because its meaning is not confirmed and it is likely a sentinel.

The agreement `40.059 - 36.841 = 3.218 m³` and the Type-2 current-month value of `3.218 m³` cross-check each other.

**Invoice interpretation matters:** 36.841 m³ is a cumulative meter reading at the end of the previous month, not that month's consumption. If the invoice's 36 m³ is the meter reading, it is consistent with this frame. If the invoice says that 36 m³ was consumed during the month, it does not match the Type-2 previous-month consumption value of 11.230 m³ and that discrepancy needs checking against the exact invoice fields and a later meter sample.

## 5. Installation and validation

1. In the ESPHome dashboard, open the existing Flowis device YAML.
2. Merge the template sensors and text sensors, and replace the `on_packet` lambda as described above.
3. Save and run **Validate**.
4. If validation succeeds, compile and install OTA.
5. Open ESPHome logs. Wait for valid Type-1 and Type-2 frames with CRC acceptance.
6. In Home Assistant, verify that the cumulative sensor updates to a value near 40.059 m³ and that the current-month sensor agrees with the Type-1 difference.
7. Finally, change the source entity in the existing HA last-reading template to the new Flowis cumulative sensor.

Do not use Type-2 monthly snapshots as a replacement for the cumulative meter register in the water/energy dashboard. Use `sensor.licznik_wody_ostatni_odczyt` (or the new cumulative sensor directly) for the total meter reading.

## Important limitations

- The decoder is based on the Flowis+ ECO4 frame layout described in the current rtl_433 Flowis investigation and PR #3785. The user's own received frames prove packet lengths, serial, CRC pass and the sample values, but the complete semantics of alarm/backflow bytes and the `0xFFFF` monthly slot are not yet confirmed.
- This is a repository example, not a verified build of the user's full, private `flowis.yaml`. ESPHome Validate/compile on the target configuration is still required before treating the sensors as deployed.
- Keep the receiver's successful 868.00 MHz setting while validating the decoder. Do not change radio settings and decoder logic in the same test.
