import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_FREQUENCY
from esphome.core import CORE

CODEOWNERS = []
AUTO_LOAD = ["json"]
DEPENDENCIES = ["esp32"]

rtl433_ns = cg.esphome_ns.namespace("rtl433")
Rtl433Component = rtl433_ns.class_("Rtl433Component", cg.Component)
Rtl433MessageTrigger = rtl433_ns.class_(
    "Rtl433MessageTrigger", automation.Trigger.template(cg.std_string)
)

CONF_CS_PIN = "cs_pin"
CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_CLK_PIN = "clk_pin"
CONF_MISO_PIN = "miso_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_ON_MESSAGE = "on_message"
CONF_MODULATION = "modulation"
CONF_BITRATE = "bitrate"
CONF_DEVIATION = "deviation"
CONF_RX_BANDWIDTH = "rx_bandwidth"
CONF_FLOWIS_DEBUG = "flowis_debug"
CONF_MIN_PULSE_LENGTH = "min_pulse_length"

# --- New config keys (rtl_433_ESP tuning options) ---
CONF_MIN_RSSI = "min_rssi"
CONF_RSSI_THRESHOLD_DELTA = "rssi_threshold_delta"
CONF_RSSI_SAMPLES = "rssi_samples"
CONF_PUBLISH_UNPARSED = "publish_unparsed"
CONF_RTL_DEBUG = "rtl_debug"
CONF_DEAF_WORKAROUND = "deaf_workaround"
CONF_AUTO_RSSI_THRESHOLD = "auto_rssi_threshold"
CONF_LOG_MODULE_STATUS = "log_module_status"
CONF_RAW_SIGNAL_DEBUG = "raw_signal_debug"
CONF_MEMORY_DEBUG = "memory_debug"
CONF_DEMOD_DEBUG = "demod_debug"
CONF_SIGNAL_RSSI = "signal_rssi"
CONF_STACK_DEBUG = "stack_debug"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Rtl433Component),
        cv.Required(CONF_CS_PIN): cv.int_range(min=0, max=48),
        cv.Required(CONF_GDO0_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_GDO2_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_CLK_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_MISO_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_MOSI_PIN): cv.int_range(min=0, max=48),
        cv.Optional(CONF_FREQUENCY, default=433.92): cv.float_range(min=300.0, max=928.0),
        cv.Optional(CONF_MODULATION, default="OOK"): cv.one_of("OOK", "FSK", upper=True),
        # CC1101 FSK modem settings (RadioLib 7.2.1 units: kbps / kHz).
        cv.Optional(CONF_BITRATE, default=17.24): cv.float_range(min=0.025, max=600.0),
        cv.Optional(CONF_DEVIATION, default=40.0): cv.float_range(min=1.587, max=380.8),
        cv.Optional(CONF_RX_BANDWIDTH, default=270.0): cv.float_range(min=58.0, max=812.0),
        # Optional override for rtl_433_ESP pulse capture filter (microseconds).
        cv.Optional(CONF_MIN_PULSE_LENGTH): cv.int_range(min=1, max=1000),
        # Enable verbose output only for the Flowis decoder in rtl_433_ESP.
        cv.Optional(CONF_FLOWIS_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(Rtl433MessageTrigger),
            }
        ),
        # -- RSSI & signal detection --
        cv.Optional(CONF_MIN_RSSI, default=-82): cv.int_range(min=-120, max=0),
        cv.Optional(CONF_RSSI_THRESHOLD_DELTA, default=9): cv.int_range(min=0, max=50),
        cv.Optional(CONF_RSSI_SAMPLES, default=50000): cv.int_range(min=100, max=500000),
        cv.Optional(CONF_AUTO_RSSI_THRESHOLD, default=True): cv.boolean,
        # -- Unparsed / debug --
        cv.Optional(CONF_PUBLISH_UNPARSED, default=False): cv.boolean,
        cv.Optional(CONF_RTL_DEBUG, default=0): cv.int_range(min=0, max=4),
        # -- CC1101 workaround --
        cv.Optional(CONF_DEAF_WORKAROUND, default=True): cv.boolean,
        # -- Startup diagnostics --
        cv.Optional(CONF_LOG_MODULE_STATUS, default=False): cv.boolean,
        # -- Verbose debug flags (all default off) --
        cv.Optional(CONF_RAW_SIGNAL_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_MEMORY_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_DEMOD_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_SIGNAL_RSSI, default=False): cv.boolean,
        cv.Optional(CONF_STACK_DEBUG, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # ESPHome 2026.2+ selectively compiles Arduino libraries. RadioLib's
    # Arduino HAL includes SPI.h, so explicitly enable the bundled SPI library
    # when building with the Arduino framework.
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("SPI", None)

    # Inject library dependencies so users don't need to add them manually.
    cg.add_library(
        "rtl_433_ESP",
        None,
        "https://github.com/NorthernMan54/rtl_433_ESP.git#f0a81569b58a60ea8b3a5efd8e9b39670e8d8df8",
    )
    # Pin the version whose CC1101 tuning API uses kbps/kHz as documented.
    cg.add_library("jgromes/RadioLib", "7.7.1")
    cg.add_platformio_option("lib_ldf_mode", "chain+")

    # Always activate the CC1101 path in rtl_433_ESP
    cg.add_build_flag("-DRF_CC1101")

    # Required CC1101 pins
    cg.add_build_flag(f"-DRF_MODULE_CS={config[CONF_CS_PIN]}")
    cg.add_build_flag(f"-DRF_MODULE_GDO0={config[CONF_GDO0_PIN]}")

    # GDO2 is optional: rtl_433_ESP only uses GDO0 for interrupt/data at
    # runtime.  RadioLib's Module() constructor accepts RADIOLIB_NC for the
    # gpio parameter and silently disables GDO2-related functionality.
    if CONF_GDO2_PIN in config:
        cg.add_build_flag(f"-DRF_MODULE_GDO2={config[CONF_GDO2_PIN]}")
    else:
        cg.add_build_flag("-DRF_MODULE_GDO2=RADIOLIB_NC")

    # Optional custom SPI bus — all four must be present for rtl_433_ESP to use newSPI
    has_custom_spi = all(
        k in config for k in (CONF_CLK_PIN, CONF_MISO_PIN, CONF_MOSI_PIN)
    )
    if has_custom_spi:
        cg.add_build_flag(f"-DRF_MODULE_SCK={config[CONF_CLK_PIN]}")
        cg.add_build_flag(f"-DRF_MODULE_MISO={config[CONF_MISO_PIN]}")
        cg.add_build_flag(f"-DRF_MODULE_MOSI={config[CONF_MOSI_PIN]}")

    # Frequency
    cg.add_build_flag(f"-DRF_MODULE_FREQUENCY={config[CONF_FREQUENCY]}")

    # Override the library pulse-width filter only when explicitly configured.
    # The library defaults are 50 us for OOK and 30 us for FSK.
    if CONF_MIN_PULSE_LENGTH in config:
        cg.add_build_flag(f"-DMINIMUM_PULSE_LENGTH={config[CONF_MIN_PULSE_LENGTH]}")

    # rtl_433_ESP FSK protocol table in the pinned source places Flowis at
    # index 56. RTL_VERBOSE enables per-device verbosity for only that decoder.
    if config[CONF_FLOWIS_DEBUG]:
        cg.add_build_flag("-DRTL_VERBOSE=56")

    # Modulation: OOK (default) or FSK
    ook = "true" if config[CONF_MODULATION] == "OOK" else "false"
    cg.add_build_flag(f"-DOOK_MODULATION={ook}")

    # rtl_433_ESP exposes these CC1101 tuning wrappers behind test flags.
    # Enable them only for FSK, then set the requested values before the
    # receiver interrupt is enabled by the component.
    if config[CONF_MODULATION] == "FSK":
        cg.add_build_flag("-DsetBitrate")
        cg.add_build_flag("-DsetFreqDev")
        cg.add_build_flag("-DsetRxBW")
        cg.add_build_flag("-DFLOWIS_RTL433_FSK")

    # ---- RSSI & signal detection (compile-time build flags) ----
    cg.add_build_flag(f"-DMINRSSI={config[CONF_MIN_RSSI]}")
    cg.add_build_flag(f"-DRSSI_THRESHOLD={config[CONF_RSSI_THRESHOLD_DELTA]}")
    cg.add_build_flag(f"-DRSSI_SAMPLES={config[CONF_RSSI_SAMPLES]}")

    # Auto RSSI threshold: upstream uses DISABLERSSITHRESHOLD to suppress AUTORSSITHRESHOLD
    if not config[CONF_AUTO_RSSI_THRESHOLD]:
        cg.add_build_flag("-DDISABLERSSITHRESHOLD")

    # Runtime setter for rssi_threshold_delta — ensures the value is applied
    # even if the library re-initialises the static after the build flag is read.
    cg.add(var.set_rssi_threshold_delta(config[CONF_RSSI_THRESHOLD_DELTA]))
    if config[CONF_MODULATION] == "FSK":
        cg.add(var.set_fsk_bitrate(config[CONF_BITRATE]))
        cg.add(var.set_fsk_deviation(config[CONF_DEVIATION]))
        cg.add(var.set_fsk_rx_bandwidth(config[CONF_RX_BANDWIDTH]))

    # ---- Unparsed signals (compile-time #ifdef) ----
    if config[CONF_PUBLISH_UNPARSED]:
        cg.add_build_flag("-DPUBLISH_UNPARSED")

    # ---- RTL debug verbosity (compile-time; setDebug() is documented as non-functional) ----
    if config[CONF_RTL_DEBUG] > 0:
        cg.add_build_flag(f"-DRTL_DEBUG={config[CONF_RTL_DEBUG]}")

    # ---- CC1101 deaf workaround (compile-time) ----
    if not config[CONF_DEAF_WORKAROUND]:
        cg.add_build_flag("-DNO_DEAF_WORKAROUND")

    # ---- Startup diagnostics (compile-time) ----
    if config[CONF_LOG_MODULE_STATUS]:
        cg.add_build_flag("-DRF_MODULE_INIT_STATUS")

    # ---- Verbose debug flags (compile-time #ifdef, only emitted when true) ----
    if config[CONF_RAW_SIGNAL_DEBUG]:
        cg.add_build_flag("-DRAW_SIGNAL_DEBUG")
    if config[CONF_MEMORY_DEBUG]:
        cg.add_build_flag("-DMEMORY_DEBUG")
    if config[CONF_DEMOD_DEBUG]:
        cg.add_build_flag("-DDEMOD_DEBUG")
    if config[CONF_SIGNAL_RSSI]:
        cg.add_build_flag("-DSIGNAL_RSSI")
    if config[CONF_STACK_DEBUG]:
        cg.add_build_flag("-DSTACK_DEBUG")

    # Register on_message triggers
    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
