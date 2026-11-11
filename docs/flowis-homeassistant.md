# Flowis+ ECO: decoded values in ESPHome and Home Assistant

This guide adds decoded Flowis values on top of the working CC1101 packet-mode receiver.
It is intended for ESPHome 2026.10.2 and the branch
`experiment/flowis-cc1101-packet-diagnostics`.

Complete example:
[`example-flowis-cc1101-homeassistant.yaml`](../example-flowis-cc1101-homeassistant.yaml)

## 1. Important: verify the meter ID

The captured Type-1 frame in the current test contains the ID bytes:

```
04 17 5E 0E
```

The Flowis protocol stores the ID little-endian, so this decodes to **241047300**.

The earlier Home Assistant source entity `sensor.flowis_241047064_water_usage` used an
incorrect meter ID and should be ignored. The current captured frames decode consistently
to **241047300**, and the example filters for that ID.

The example filters for meter ID `241047300`. In the `on_packet` lambda, change:

```cpp
constexpr uint32_t EXPECTED_METER_ID = 241047300UL;
```

only if the physical meter is confirmed to have another ID. Set it to `0` to temporarily
accept any Flowis meter received by the radio (not recommended if neighbouring meters are
within range).

## 2. ESPHome: where to make the changes

Open the existing `flowis.yaml` in the ESPHome dashboard.

Keep the working radio configuration and wiring. Do not add a second `rtl433:` or
`rtl_433_ESP` component; the native `cc1101:` component is the only owner of the radio.

In the `cc1101:` section:

- Keep `frequency: 868.00MHz` (the frequency at which valid CRC-checked frames were received).
- Keep the current 2-FSK, 100 kbaud, 50 kHz deviation, 325 kHz filter, 203 kHz IF and D3/91 sync settings.
- Keep `packet_mode: true`, `packet_length: 0` and `crc_enable: true`.
- Replace the existing `on_packet:` lambda with the one from the complete example.

Then add the example's `sensor:` and `text_sensor:` template entities at the top level of
the ESPHome YAML file. If those sections already exist, merge the entries into them; do not
create a second top-level `sensor:` or `text_sensor:` key.

The callback:
- accepts Type 1 only when the payload is exactly 25 bytes;
- accepts Type 2 only when the payload is exactly 19 bytes;
- receives packets only after the CC1101 hardware CRC check succeeds;
- publishes the lifetime total and previous-month-end total from Type 1;
- calculates current-month use independently as lifetime total minus previous-month-end total;
- decodes seven Type-2 monthly values as little-endian 16-bit litres;
- ignores `0xFFFF` as an invalid/special monthly value and does not replace the prior state with it;
- publishes the raw alarm/backflow bytes without guessing their meaning.

After saving, run **Validate** first. Then install wirelessly and watch the ESPHome logs.

## 3. ESPHome entities created

| ESPHome entity name | Meaning |
|---|---|
| Flowis stan całkowity | Lifetime meter reading from Type 1, m³ |
| Flowis stan na koniec poprzedniego miesiąca | Lifetime reading at previous month end, m³ |
| Flowis zużycie bieżący miesiąc | Current calendar month, m³ |
| Flowis zużycie miesiąc -1 through -6 | Previous six monthly values, m³ |
| Flowis numer licznika | Meter ID decoded from the frame |
| Flowis czas licznika | Timestamp encoded by the meter |
| Flowis RSSI | Received signal strength, dBm |
| Flowis przesunięcie częstotliwości | CC1101 frequency estimate, Hz |
| Flowis alarm raw / backflow raw | Raw Type-1 status bytes; interpretation not yet confirmed |

With the example names, the lifetime source entity will normally be
`sensor.flowis_stan_calkowity`. Confirm the exact entity ID in Home Assistant under
**Developer Tools → States**; Home Assistant may preserve an earlier entity ID or add a suffix
if an entity with the same ID already exists.

## 4. Home Assistant: preserve the existing “last reading” entity

Find the existing template sensor with:

- `unique_id: licznik_wody_ostatni_odczyt`
- name `Licznik wody (ostatni odczyt)`

Do **not** add a second sensor with the same unique ID. In its existing state template,
replace the old source entity with the new ESPHome source. Keep the same name, unique ID,
unit and classes:

```yaml
template:
  - sensor:
      - name: "Licznik wody (ostatni odczyt)"
        unique_id: licznik_wody_ostatni_odczyt
        unit_of_measurement: "m³"
        device_class: water
        state_class: total_increasing
        state: >
          {% set current = states('sensor.flowis_stan_calkowity') %}
          {% if is_number(current) and current | float > 0 %}
            {{ current | float }}
          {% elif is_number(this.state) %}
            {{ this.state | float }}
          {% else %}
            {{ none }}
          {% endif %}
```

If the existing `template:` section already exists in `configuration.yaml` or an included
file, edit that existing sensor in place. Do not paste a second root-level `template:` key
unless your configuration uses a separate included file.

This template accepts only numeric values greater than zero from the new source. When the
source is `unknown`, `unavailable`, empty or zero, it keeps the prior numeric state. Before
the entity has ever received a valid reading, it remains `unknown` rather than fabricating
a numeric zero.

After editing, use **Developer Tools → YAML → Check configuration**, then reload Template
Entities or restart Home Assistant if the UI offers no reload action for your configuration.

## 5. Expected decoded values from the sample log

The captured sample frames decode as follows:

- Meter ID: **241047300**
- Type-1 lifetime total: **40.059 m³**
- Type-1 previous-month-end total: **36.841 m³**
- Current-month difference from Type 1: **3.218 m³**
- Type-2 current month: **3.218 m³**
- Type-2 previous six months: **11.230, 10.131, 10.617, 4.612, 0.001 m³**, then one `0xFFFF` special/invalid value (not published).

The Type-1 and Type-2 current-month values agree:
`40.059 - 36.841 = 3.218 m³`.

The 36.841 m³ Type-1 value is a cumulative meter reading at the end of the previous month,
not the amount of water used during that month. Type-2 reports the monthly consumption
history separately; in this captured frame, the previous calendar month slot is 11.230 m³.
If a bill says 36 m³ of *consumption* rather than a cumulative meter reading, compare its
billing period and start/end readings before treating that value as the same measurement.

This confirms the byte offsets for the sample frame. It does not establish the meaning of the
raw alarm/backflow bytes, so they are intentionally exposed as raw numeric values.

## 6. First verification

After the ESPHome node reconnects, confirm that:

1. `sensor.flowis_stan_calkowity` shows the meter's physical reading (after checking the meter ID).
2. `sensor.flowis_zuzycie_biezacy_miesiac` shows the current month's consumption.
3. `sensor.licznik_wody_ostatni_odczyt` retains its last good value if the receiver is offline
   or no valid packet arrives.
4. The ESPHome log contains `FLOWIS_DIAG PACKET_ACCEPT` and `flowis_decode TYPE1/TYPE2`.

The previous entity ID `241047064` was identified as incorrect configuration and is not used
by this example. The example currently filters the received meter ID `241047300`.
