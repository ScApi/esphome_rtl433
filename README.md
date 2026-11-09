# ESPHome rtl_433 CC1101 Component

An [ESPHome](https://esphome.io/) external component that turns an ESP32 + CC1101 RF transceiver into a configurable sub-GHz sensor gateway (for example 433 MHz or 868 MHz). Decoded messages from weather stations, door/window sensors, tire-pressure monitors, and hundreds of other devices are delivered through ESPHome's native automation system -- no MQTT broker required.


Under the hood the component wraps [rtl_433_ESP](https://github.com/NorthernMan54/rtl_433_ESP), which is a port of the widely-used [rtl_433](https://github.com/merbanan/rtl_433) project.

## Features

- **CC1101 transceiver support** -- wired over SPI to any ESP32.
- **Native sensor platforms** -- declare `sensor`, `text_sensor`, and `binary_sensor` components directly in YAML. No lambdas or JSON parsing required.
- **No MQTT dependency** -- messages fire an `on_message` automation trigger carrying the raw JSON string, or use the native sensor platforms to publish directly to Home Assistant over the native API.
- **Zero manual library setup** -- the component auto-injects `rtl_433_ESP`, `RadioLib`, and the required PlatformIO flags.
- **OOK and FSK modulation** -- OOK by default; switch to FSK with `modulation: FSK` in your YAML.

## Hardware Requirements

| Item | Notes |
|------|-------|
| ESP32 dev board | Any variant (ESP32, ESP32-S3, etc.) running Arduino framework |
| CC1101 RF transceiver module | Widely available; make sure you get the 433 MHz version |
| Jumper wires / PCB | Standard 2.54 mm header connections |

> **Note:** This component targets the CC1101 only. ESPHome already has native support for SX127x-based radios, so that hardware path is not duplicated here.

## Wiring

Connect the CC1101 module to the ESP32. The table below shows the default VSPI wiring that matches the example configuration.

| CC1101 Pin | ESP32 GPIO | Config Key | Required |
|------------|------------|------------|----------|
| VCC        | 3.3 V      | --         | Yes      |
| GND        | GND        | --         | Yes      |
| CSN        | GPIO 5     | `cs_pin`   | Yes      |
| GDO0       | GPIO 13    | `gdo0_pin` | Yes      |
| GDO2       | GPIO 4     | `gdo2_pin` | No       |
| SCK        | GPIO 18    | `clk_pin`  | No (VSPI default) |
| MOSI       | GPIO 23    | `mosi_pin` | No (VSPI default) |
| MISO       | GPIO 19    | `miso_pin` | No (VSPI default) |

If you omit `clk_pin`, `miso_pin`, and `mosi_pin`, the component uses the ESP32 hardware VSPI defaults. Supply all three to use a custom SPI bus.

> **Note:** `gdo2_pin` is optional. The CC1101 receiver path uses only GDO0 for interrupt-driven data capture; GDO2 is not read at runtime. If omitted, `RADIOLIB_NC` (not connected) is passed to RadioLib and GDO2 functionality is silently disabled. You only need to set it if your hardware or a future library feature requires GDO2.

## Installation

Add the component as an external source in your ESPHome YAML. Point it at this repository (or a local checkout):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/<your-org>/esphome_rtl433.git
    components: [rtl433]
```

For a local checkout:

```yaml
external_components:
  - source:
      type: local
      path: components
```

No additional `lib_deps` or `platformio_options` entries are needed for basic operation -- the component injects them automatically.

## Configuration Reference

```yaml
rtl433:
  cs_pin: 5            # Required  int (0-48)  -- CC1101 CSN / chip-select pin
  gdo0_pin: 13         # Required  int (0-48)  -- CC1101 GDO0 (interrupt / data)
  # gdo2_pin: 4        # Optional  int (0-48)  -- CC1101 GDO2 (defaults to RADIOLIB_NC)
  clk_pin: 18          # Optional  int (0-48)  -- SPI SCK  (omit for VSPI default)
  miso_pin: 19         # Optional  int (0-48)  -- SPI MISO (omit for VSPI default)
  mosi_pin: 23         # Optional  int (0-48)  -- SPI MOSI (omit for VSPI default)
  frequency: 433.92    # Optional  float (300.0-928.0)  -- default 433.92 MHz
  modulation: OOK      # Optional  "OOK" or "FSK"       -- default OOK
  # min_rssi: -82      # Optional  int (-120 to 0)  -- minimum RSSI threshold in dBm
  # rssi_threshold_delta: 9  # Optional  int (0-50)  -- delta above average RSSI
  # rssi_samples: 50000      # Optional  int (100-500000)  -- samples for RSSI average
  # auto_rssi_threshold: true  # Optional  bool  -- auto-adjust RSSI threshold
  # publish_unparsed: false    # Optional  bool  -- fire on_message for undecoded signals
  # rtl_debug: 0               # Optional  int (0-4)  -- decoder debug verbosity
  # deaf_workaround: true      # Optional  bool  -- CC1101 hourly re-init workaround
  # log_module_status: false   # Optional  bool  -- log transceiver config at startup
  # raw_signal_debug: false    # Optional  bool  -- log raw signal data
  # memory_debug: false        # Optional  bool  -- log heap usage
  # demod_debug: false         # Optional  bool  -- log demodulation details
  # signal_rssi: false         # Optional  bool  -- track per-pulse RSSI
  # stack_debug: false         # Optional  bool  -- log stack high-water marks
  on_message:          # Optional  automation trigger -- fires with raw JSON string `x`
    - lambda: |-
        ESP_LOGI("rtl433", "%s", x.c_str());
```

### Config keys

#### Hardware & radio

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `cs_pin` | int (0--48) | Yes | -- | CC1101 chip-select (CSN) GPIO |
| `gdo0_pin` | int (0--48) | Yes | -- | CC1101 GDO0 GPIO (interrupt / data line) |
| `gdo2_pin` | int (0--48) | No | `RADIOLIB_NC` | CC1101 GDO2 GPIO. Not used at runtime; safe to omit. |
| `clk_pin` | int (0--48) | No | VSPI default | SPI clock GPIO |
| `miso_pin` | int (0--48) | No | VSPI default | SPI MISO GPIO |
| `mosi_pin` | int (0--48) | No | VSPI default | SPI MOSI GPIO |
| `frequency` | float (300--928) | No | 433.92 | Receive frequency in MHz |
| `modulation` | string | No | `OOK` | RF modulation type: `OOK` or `FSK`. The CC1101 can only receive one type at a time. Most 433 MHz devices use OOK; TPMS and some industrial sensors use FSK. |
| `bitrate` | float (0.025--600) | No | `17.24` | CC1101 FSK bit rate in kbps. Applied only when `modulation: FSK`. |
| `deviation` | float (1.587--380.8) | No | `40` | CC1101 FSK frequency deviation in kHz. Applied only when `modulation: FSK`. |
| `rx_bandwidth` | float (58--812) | No | `270` | CC1101 FSK receiver bandwidth in kHz. RadioLib selects the nearest supported setting. Applied only when `modulation: FSK`. |
| `on_message` | automation | No | -- | Trigger that fires with each decoded message; the variable `x` is a `std::string` containing the raw JSON |

#### RSSI & signal detection

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `min_rssi` | int (-120 to 0) | No | -82 | Minimum RSSI in dBm. Signals weaker than this are discarded. Lower values (e.g. -90) are more permissive; higher values (e.g. -70) require stronger signals. Maps to the `MINRSSI` build flag. |
| `rssi_threshold_delta` | int (0--50) | No | 9 | Delta in dB added to the average background RSSI to determine whether a signal is present. Increase to reduce false triggers in noisy environments. Maps to `RSSI_THRESHOLD` build flag and `setRSSIThreshold()` runtime call. |
| `rssi_samples` | int (100--500000) | No | 50000 | Number of RSSI readings used to compute the background average. Higher values smooth out transient noise but respond more slowly to environment changes. Maps to `RSSI_SAMPLES` build flag. |
| `auto_rssi_threshold` | bool | No | `true` | When true, the RSSI threshold is continuously adjusted based on the background signal level. When false, the fixed `min_rssi` value is used as-is. Maps to `DISABLERSSITHRESHOLD` / `AUTORSSITHRESHOLD` build flags. |

#### Decoder behaviour

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `publish_unparsed` | bool | No | `false` | When true, the `on_message` trigger fires even for signals that could not be decoded by any protocol handler. The JSON will contain `"model":"undecoded signal"`. Useful for debugging whether the radio is receiving anything. Maps to `PUBLISH_UNPARSED` build flag. |
| `rtl_debug` | int (0--4) | No | 0 | rtl_433 decoder verbosity level: 0 = normal, 1 = verbose, 2 = verbose decoders, 3 = debug decoders, 4 = trace decoding. Increases log output and decoder stack size. Maps to `RTL_DEBUG` build flag. |

#### CC1101 workaround

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `deaf_workaround` | bool | No | `true` | Enable the hourly receiver re-initialisation that works around a known CC1101 bug where the transceiver stops receiving after extended operation. Set to false only if you experience stability issues from the re-init. Maps to `NO_DEAF_WORKAROUND` build flag. |

#### Startup diagnostics

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `log_module_status` | bool | No | `false` | When true, logs the SPI pin configuration and transceiver register status during startup. Helpful for verifying wiring. Maps to `RF_MODULE_INIT_STATUS` build flag. |

#### Verbose debug flags

These are compile-time debug switches. All default to `false`. Enabling them increases log output and may affect performance.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `raw_signal_debug` | bool | No | `false` | Log raw received signal data. Maps to `RAW_SIGNAL_DEBUG` build flag. |
| `memory_debug` | bool | No | `false` | Log heap usage at key points during init and decoding. Maps to `MEMORY_DEBUG` build flag. |
| `demod_debug` | bool | No | `false` | Log demodulation details (GPIO pin, frequency, signal lengths). Maps to `DEMOD_DEBUG` build flag. |
| `signal_rssi` | bool | No | `false` | Track and store per-pulse RSSI values. Maps to `SIGNAL_RSSI` build flag. |
| `stack_debug` | bool | No | `false` | Log FreeRTOS task stack high-water marks. Maps to `STACK_DEBUG` build flag. |

## Native Sensor Platforms

Instead of writing C++ lambdas to parse JSON, you can declare sensors directly in YAML. The rtl433 component will automatically match decoded messages by model and (optionally) device ID, extract the specified field, and publish the value.

### Sensor (numeric values)

```yaml
sensor:
  - platform: rtl433
    name: "Outdoor Temperature"
    device_model: "Acurite-606TX"       # Required -- must match the "model" field in decoded JSON
    device_id: "1234"                   # Optional -- match a specific device ID
    field: temperature_C                # Required -- JSON key to read the float value from
    unit_of_measurement: "C"
    device_class: temperature
    accuracy_decimals: 1
```

All standard ESPHome sensor config keys are supported: `unit_of_measurement`, `device_class`, `accuracy_decimals`, `filters`, `state_class`, etc.

Use `filters` to convert units inline:

```yaml
sensor:
  - platform: rtl433
    name: "Tire Pressure"
    device_model: "Schrader-EG53MA4"
    device_id: "0A1B2C"
    field: pressure_kPa
    unit_of_measurement: "psi"
    filters:
      - multiply: 0.145038
    device_class: pressure
```

### Text Sensor (string values)

```yaml
text_sensor:
  - platform: rtl433
    name: "Last Device Model"
    device_model: "Acurite-606TX"
    field: model
```

### Binary Sensor (boolean/truthy values)

For fields that represent on/off states (e.g., battery status, motion flags). Non-zero, non-null, non-empty values are treated as ON.

```yaml
binary_sensor:
  - platform: rtl433
    name: "Battery OK"
    device_model: "Acurite-606TX"
    field: battery_ok
    device_class: battery
```

### Platform config keys

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `rtl433_id` | ID | No | Parent rtl433 component ID. Only needed if you have multiple rtl433 instances (not currently supported, but future-proof). |
| `device_model` | string | Yes | Must match the `model` field in decoded JSON (e.g., `"Acurite-606TX"`, `"Schrader-EG53MA4"`). |
| `device_id` | string | No | Match a specific device by its `id` field. If omitted, accepts any device of the specified model. |
| `field` | string | Yes | The JSON key to extract the value from (e.g., `temperature_C`, `pressure_kPa`, `battery_ok`). |

Plus all standard ESPHome config keys for the respective platform (sensor, text_sensor, binary_sensor).

### Finding field names

The `field` value must match a JSON key from the decoded message. The easiest way to discover available fields is to add an `on_message` lambda that logs the raw JSON (see the [troubleshooting guide](docs/troubleshooting.md#tip-log-raw-json-to-find-correct-names)). Common fields include `temperature_C`, `humidity`, `pressure_kPa`, `battery_ok`, `id`, and `model`, but the exact set depends on the device. See [docs/supported-devices.md](docs/supported-devices.md#finding-field-names-for-your-device) for a full walkthrough and example JSON.

## Usage Examples (on_message lambda)

The `on_message` trigger is still fully supported for advanced use cases. The native sensor platforms and `on_message` work side by side.

### Log all received messages

```yaml
rtl433:
  cs_pin: 5
  gdo0_pin: 13
  on_message:
    - lambda: |-
        ESP_LOGI("rtl433", "Received: %s", x.c_str());
```

### Filter by device model

```yaml
rtl433:
  cs_pin: 5
  gdo0_pin: 13
  on_message:
    - lambda: |-
        if (x.find("\"model\":\"Acurite-606TX\"") != std::string::npos) {
          ESP_LOGI("rtl433", "Acurite sensor: %s", x.c_str());
        }
```

### Publish to a template text sensor (visible in Home Assistant)

This approach makes the last raw message available as a Home Assistant entity. Add the `json` component if you want to parse fields.

```yaml
api:

json:

text_sensor:
  - platform: template
    name: "Last RTL433 Message"
    id: last_rtl433_message

rtl433:
  cs_pin: 5
  gdo0_pin: 13
  on_message:
    - lambda: |-
        id(last_rtl433_message).publish_state(x);
```

### Parse JSON and publish a temperature sensor to Home Assistant

```yaml
api:

json:

sensor:
  - platform: template
    name: "Outdoor Temperature"
    id: outdoor_temp
    unit_of_measurement: "C"
    accuracy_decimals: 1

rtl433:
  cs_pin: 5
  gdo0_pin: 13
  on_message:
    - lambda: |-
        json::parse_json(x, [](JsonObject root) -> bool {
          if (root["model"] == "Acurite-606TX") {
            float temp = root["temperature_C"];
            id(outdoor_temp).publish_state(temp);
          }
          return true;
        });
```

### Send a Home Assistant event for every message

```yaml
api:

rtl433:
  cs_pin: 5
  gdo0_pin: 13
  on_message:
    - homeassistant.event:
        event: esphome.rtl433_message
        data:
          payload: !lambda 'return x;'
```

## rtl_433_ESP Build Flags

Most rtl_433_ESP build flags are now exposed as first-class YAML config keys (see **Config keys** above). The component injects the correct `-D` flags automatically.

For the rare flags that are **not** exposed as config keys, you can still pass them manually:

```yaml
esphome:
  platformio_options:
    build_flags:
      - "-DMY_DEVICES=true"         # Only register devices in your custom list (smaller binary)
```

| Flag | Default | Description |
|------|---------|-------------|
| `MY_DEVICES` | `false` | When `true`, only registers devices from a custom device list (saves flash). Requires a custom devices header; cannot be abstracted into a simple YAML key. |

## OOK vs FSK

Most common 433 MHz devices (weather stations, door/window sensors, soil moisture sensors) use **OOK** (On-Off Keying) modulation. Some devices -- notably tire-pressure monitoring systems (TPMS) and certain industrial sensors -- use **FSK** (Frequency Shift Keying).

The CC1101 can only receive one modulation type at a time. By default, the component starts in OOK mode. To switch to FSK, set `modulation: FSK` in your YAML:

```yaml
rtl433:
  cs_pin: 5
  gdo0_pin: 13
  modulation: FSK
```

## CC1101 FSK modem tuning

The component exposes the CC1101 modem settings as YAML options. The values are applied after the receiver is initialised but before its capture interrupt is enabled; after changing the modem registers, asynchronous receive mode is restarted. These options are only used with `modulation: FSK`.

```yaml
rtl433:
  frequency: 868.00
  modulation: FSK
  bitrate: 100          # kbps
  deviation: 50         # kHz
  rx_bandwidth: 325     # kHz
```

The component pins RadioLib to version `7.2.1`, whose CC1101 setters use kbps/kHz units. This avoids silently changing units when a newer RadioLib release changes the method signatures.

## Frequency Selection

The default frequency is **433.92 MHz**, which covers the vast majority of consumer devices in ITU Region 1 (Europe, Africa) and many devices worldwide. Other common frequencies:

- **315.00 MHz** -- common in North America for older devices
- **868.30 MHz** -- used by some European devices (requires an 868 MHz CC1101 module)
- **915.00 MHz** -- ISM band in North America / Australia

Make sure your CC1101 module's hardware is rated for the frequency you select. A 433 MHz module will have poor or no reception at 868/915 MHz and vice versa.

## RSSI Tuning

If you receive too many false positives or noise triggers, adjust the `min_rssi` config key to filter out weak signals. The default is `-82` dBm. Lower the value (e.g., `-90`) to be more permissive, or raise it (e.g., `-70`) to require stronger signals.

You can also increase `rssi_threshold_delta` (default `9`) to require a larger gap between background noise and a valid signal. If you want to disable adaptive thresholding entirely, set `auto_rssi_threshold: false`.

```yaml
rtl433:
  cs_pin: 5
  gdo0_pin: 13
  min_rssi: -75
  rssi_threshold_delta: 12
```

## Supported Devices

This component supports all devices that the upstream rtl_433_ESP library can decode. See [docs/supported-devices.md](docs/supported-devices.md) for details.

## Troubleshooting

See [docs/troubleshooting.md](docs/troubleshooting.md) for common issues and solutions.

## Links

- [rtl_433_ESP](https://github.com/NorthernMan54/rtl_433_ESP) -- the underlying ESP32 port of rtl_433
- [rtl_433](https://github.com/merbanan/rtl_433) -- the original rtl_433 project
- [RadioLib](https://github.com/jgromes/RadioLib) -- radio abstraction library used by rtl_433_ESP
- [ESPHome External Components](https://esphome.io/components/external_components.html) -- ESPHome documentation on external components

## License

This component is provided as-is. See the upstream libraries for their respective licenses.
