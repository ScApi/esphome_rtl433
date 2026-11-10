#include "cc1101.h"
#include "cc1101pa.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::cc1101 {

static const char *const TAG = "cc1101";

static void split_float(float value, int mbits, uint8_t &e, uint32_t &m) {
  int e_tmp;
  float m_tmp = std::frexp(value, &e_tmp);
  if (e_tmp <= mbits) {
    e = 0;
    m = 0;
    return;
  }
  e = static_cast<uint8_t>(e_tmp - mbits - 1);
  m = static_cast<uint32_t>(((m_tmp * 2 - 1) * (1 << (mbits + 1))) + 1) >> 1;
  if (m == (1UL << mbits)) {
    e = e + 1;
    m = 0;
  }
}

CC1101Component::CC1101Component() {
  // Datasheet defaults
  memset(&this->state_, 0, sizeof(this->state_));
  this->state_.GDO2_CFG = 0x0D;  // Serial Data (for RX on GDO2)
  this->state_.GDO1_CFG = 0x2E;
  this->state_.GDO0_CFG = 0x0D;  // Serial Data (for RX on GDO0 / TX Input)
  this->state_.FIFO_THR = 7;
  this->state_.SYNC1 = 0xD3;
  this->state_.SYNC0 = 0x91;
  this->state_.PKTLEN = 0xFF;
  this->state_.APPEND_STATUS = 1;
  this->state_.LENGTH_CONFIG = 1;
  this->state_.CRC_EN = 1;
  this->state_.WHITE_DATA = 1;
  this->state_.FREQ_IF = 0x0F;
  this->state_.FREQ2 = 0x1E;
  this->state_.FREQ1 = 0xC4;
  this->state_.FREQ0 = 0xEC;
  this->state_.DRATE_E = 0x0C;
  this->state_.CHANBW_E = 0x02;
  this->state_.DRATE_M = 0x22;
  this->state_.SYNC_MODE = 2;
  this->state_.CHANSPC_E = 2;
  this->state_.NUM_PREAMBLE = 2;
  this->state_.CHANSPC_M = 0xF8;
  this->state_.DEVIATION_M = 7;
  this->state_.DEVIATION_E = 4;
  this->state_.RX_TIME = 7;
  this->state_.CCA_MODE = 3;
  this->state_.PO_TIMEOUT = 1;
  this->state_.FOC_LIMIT = 2;
  this->state_.FOC_POST_K = 1;
  this->state_.FOC_PRE_K = 2;
  this->state_.FOC_BS_CS_GATE = 1;
  this->state_.BS_POST_KP = 1;
  this->state_.BS_POST_KI = 1;
  this->state_.BS_PRE_KP = 2;
  this->state_.BS_PRE_KI = 1;
  this->state_.MAGN_TARGET = 3;
  this->state_.AGC_LNA_PRIORITY = 1;
  this->state_.FILTER_LENGTH = 1;
  this->state_.WAIT_TIME = 1;
  this->state_.HYST_LEVEL = 2;
  this->state_.WOREVT1 = 0x87;
  this->state_.WOREVT0 = 0x6B;
  this->state_.RC_CAL = 1;
  this->state_.EVENT1 = 7;
  this->state_.RC_PD = 1;
  this->state_.MIX_CURRENT = 2;
  this->state_.LODIV_BUF_CURRENT_RX = 1;
  this->state_.LNA2MIX_CURRENT = 1;
  this->state_.LNA_CURRENT = 1;
  this->state_.LODIV_BUF_CURRENT_TX = 1;
  this->state_.FSCAL3_LO = 9;
  this->state_.CHP_CURR_CAL_EN = 2;
  this->state_.FSCAL3_HI = 2;
  this->state_.FSCAL2 = 0x0A;
  this->state_.FSCAL1 = 0x20;
  this->state_.FSCAL0 = 0x0D;
  this->state_.RCCTRL1 = 0x41;
  this->state_.FSTEST = 0x59;
  this->state_.PTEST = 0x7F;
  this->state_.AGCTEST = 0x3F;
  this->state_.TEST2 = 0x88;
  this->state_.TEST1 = 0x31;
  this->state_.TEST0_LO = 1;
  this->state_.VCO_SEL_CAL_EN = 1;
  this->state_.TEST0_HI = 2;

  // PKTCTRL0
  this->state_.PKT_FORMAT = 3;
  this->state_.LENGTH_CONFIG = 2;
  this->state_.FS_AUTOCAL = 1;

  // CRITICAL: Initialize PA Table to avoid transmitting 0 power (Silence)
  memset(this->pa_table_, 0, sizeof(this->pa_table_));
}

void IRAM_ATTR CC1101Component::gpio_intr(CC1101Component *arg) { arg->enable_loop_soon_any_context(); }

void CC1101Component::setup() {
  this->spi_setup();

  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->setup();
  }

  this->configure();
  if (this->is_failed()) {
    return;
  }

  // Defer pin mode setup until after all components have completed setup()
  // This handles the case where remote_transmitter runs after CC1101 and changes pin mode
  if (this->gdo0_pin_ != nullptr) {
    this->defer([this]() {
      this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
      if (this->state_.PKT_FORMAT == static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO)) {
        this->gdo0_pin_->attach_interrupt(&CC1101Component::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
      }
    });
  }
}

void CC1101Component::configure() {
  // Manual reset sequence per CC1101 datasheet section 19.1.2
  this->cs_->digital_write(true);
  delayMicroseconds(1);
  this->cs_->digital_write(false);
  delayMicroseconds(1);
  this->cs_->digital_write(true);
  delayMicroseconds(41);
  this->cs_->digital_write(false);
  delay(5);

  this->strobe_(Command::RES);
  delay(5);

  this->read_(Register::PARTNUM);
  this->read_(Register::VERSION);
  this->chip_id_ = encode_uint16(this->state_.PARTNUM, this->state_.VERSION);
  ESP_LOGD(TAG, "CC1101 found! Chip ID: 0x%04X", this->chip_id_);
  if (this->state_.VERSION == 0 || this->state_.PARTNUM == 0xFF) {
    ESP_LOGE(TAG, "Failed to verify CC1101.");
    this->mark_failed();
    return;
  }

  this->initialized_ = true;

  for (uint8_t i = 0; i <= static_cast<uint8_t>(Register::TEST0); i++) {
    if (i == static_cast<uint8_t>(Register::FSTEST) || i == static_cast<uint8_t>(Register::AGCTEST)) {
      continue;
    }
    this->write_(static_cast<Register>(i));
  }
  this->set_output_power(this->output_power_requested_);

  if (!this->enter_rx_()) {
    this->mark_failed();
    return;
  }
}

void CC1101Component::call_listeners_(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, freq_offset, rssi, lqi);
  }
  this->packet_trigger_.trigger(packet, freq_offset, rssi, lqi);
}

void CC1101Component::loop() {
  this->disable_loop();
  if (this->state_.PKT_FORMAT != static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO) || this->gdo0_pin_ == nullptr ||
      !this->gdo0_pin_->digital_read()) {
    return;
  }

  // Read RXBYTES once and preserve the full status byte for diagnostics.
  this->read_(Register::RXBYTES);
  const uint8_t rx_bytes_raw = this->state_.RXBYTES;
  const uint8_t rx_bytes = this->state_.NUM_RXBYTES;
  const bool overflow = this->state_.RXFIFO_OVERFLOW;

  if (overflow) {
    this->read_(Register::MARCSTATE);
    ESP_LOGW(TAG,
             "FLOWIS_DIAG FIFO_OVERFLOW rxbytes_raw=0x%02X count=%u gdo0=%u marcstate=0x%02X",
             rx_bytes_raw, rx_bytes, this->gdo0_pin_->digital_read(), this->state_.MARC_STATE);
    this->enter_idle_();
    this->strobe_(Command::FRX);
    this->enter_rx_();
    return;
  }

  if (rx_bytes == 0) {
    this->read_(Register::MARCSTATE);
    ESP_LOGW(TAG,
             "FLOWIS_DIAG FIFO_EMPTY rxbytes_raw=0x%02X gdo0=%u marcstate=0x%02X",
             rx_bytes_raw, this->gdo0_pin_->digital_read(), this->state_.MARC_STATE);
    this->enter_idle_();
    this->strobe_(Command::FRX);
    this->enter_rx_();
    return;
  }

  // Read packet length. In variable-length mode the first FIFO byte is the length.
  uint8_t payload_length;
  uint16_t expected_rx;
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE)) {
    this->read_(Register::FIFO, &payload_length, 1);
    expected_rx = static_cast<uint16_t>(payload_length) + 1U;
  } else {
    payload_length = this->state_.PKTLEN;
    expected_rx = payload_length;
  }

  if (payload_length == 0 || payload_length > 64 || rx_bytes != expected_rx) {
    // Preserve a short FIFO preview before flushing. PKTSTATUS.SFD tells us
    // whether the CC1101 still sees an active packet or has reached packet end.
    this->read_(Register::PKTSTATUS);
    const uint8_t pktstatus_raw = this->state_.PKTSTATUS;
    const bool sfd_active = this->state_.SFD != 0;
    this->read_(Register::RSSI);
    const float rssi = (this->state_.RSSI * RSSI_STEP) - RSSI_OFFSET;
    this->read_(Register::MARCSTATE);

    uint8_t preview_len = rx_bytes > 1 ? static_cast<uint8_t>(rx_bytes - 1) : 0;
    if (preview_len > 48) {
      preview_len = 48;
    }
    uint8_t preview[48] = {};
    if (preview_len > 0) {
      this->read_(Register::FIFO, preview, preview_len);
    }

    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    char preview_hex[48 * 3 + 1] = {};
    size_t pos = 0;
    for (uint8_t i = 0; i < preview_len; i++) {
      if (i > 0) {
        preview_hex[pos++] = ' ';
      }
      preview_hex[pos++] = HEX_DIGITS[(preview[i] >> 4) & 0x0F];
      preview_hex[pos++] = HEX_DIGITS[preview[i] & 0x0F];
    }

    ESP_LOGW(TAG,
             "FLOWIS_DIAG INVALID_PACKET rxbytes_raw=0x%02X count=%u length=0x%02X (%u) "
             "expected=%u sfd=%u pktstatus=0x%02X rssi=%.1f gdo0=%u marcstate=0x%02X data=[%s]",
             rx_bytes_raw, rx_bytes, payload_length, payload_length, expected_rx,
             sfd_active, pktstatus_raw, rssi, this->gdo0_pin_->digital_read(),
             this->state_.MARC_STATE, preview_hex);
    this->enter_idle_();
    this->strobe_(Command::FRX);
    this->enter_rx_();
    return;
  }

  this->packet_.resize(payload_length);
  this->read_(Register::FIFO, this->packet_.data(), payload_length);

  // Read status from registers (more reliable than FIFO status bytes due to timing issues).
  this->read_(Register::FREQEST);
  this->read_(Register::RSSI);
  this->read_(Register::LQI);
  const float freq_offset = static_cast<int8_t>(this->state_.FREQEST) * (XTAL_FREQUENCY / (1 << 14));
  const float rssi = (this->state_.RSSI * RSSI_STEP) - RSSI_OFFSET;
  const bool crc_ok = (this->state_.LQI & STATUS_CRC_OK_MASK) != 0;
  const uint8_t lqi = this->state_.LQI & STATUS_LQI_MASK;
  const uint8_t crc_enable = this->state_.CRC_EN;

  if (crc_enable == 0 || crc_ok) {
    ESP_LOGI(TAG,
             "FLOWIS_DIAG PACKET_ACCEPT len=%u crc_en=%u crc_ok=%u rssi=%.1f lqi=%u freq_offset=%.0f",
             payload_length, crc_enable, crc_ok, rssi, lqi, freq_offset);
    this->call_listeners_(this->packet_, freq_offset, rssi, lqi);
  } else {
    ESP_LOGW(TAG,
             "FLOWIS_DIAG CRC_REJECT len=%u crc_en=%u lqi_raw=0x%02X lqi=%u rssi=%.1f freq_offset=%.0f",
             payload_length, crc_enable, this->state_.LQI, lqi, rssi, freq_offset);
  }

  // Return to RX, preserving the original component state transitions.
  this->enter_idle_();
  this->strobe_(Command::FRX);
  this->enter_rx_();
}
void CC1101Component::dump_config() {
  static const char *const MODULATION_NAMES[] = {"2-FSK", "GFSK",   "UNUSED", "ASK/OOK",
                                                 "4-FSK", "UNUSED", "UNUSED", "MSK"};
  int32_t freq = static_cast<int32_t>(this->state_.FREQ2 << 16 | this->state_.FREQ1 << 8 | this->state_.FREQ0) *
                 XTAL_FREQUENCY / (1 << 16);
  float symbol_rate = (((256.0f + this->state_.DRATE_M) * (1 << this->state_.DRATE_E)) / (1 << 28)) * XTAL_FREQUENCY;
  float bw = XTAL_FREQUENCY / (8.0f * (4 + this->state_.CHANBW_M) * (1 << this->state_.CHANBW_E));
  ESP_LOGCONFIG(TAG,
                "CC1101:\n"
                "  Chip ID: 0x%04X\n"
                "  Frequency: %" PRId32 " Hz\n"
                "  Channel: %u\n"
                "  Modulation: %s\n"
                "  Symbol Rate: %.0f baud\n"
                "  Filter Bandwidth: %.1f Hz\n"
                "  Output Power: %.1f dBm",
                this->chip_id_, freq, this->state_.CHANNR, MODULATION_NAMES[this->state_.MOD_FORMAT & 0x07],
                symbol_rate, bw, this->output_power_effective_);
  LOG_PIN("  CS Pin: ", this->cs_);
}

void CC1101Component::begin_tx() {
  // Read the mode before the PKTCTRL0 write below overwrites it in state_
  const bool packet_mode = this->state_.PKT_FORMAT == static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO);
  // Ensure Packet Format is 3 (Async Serial)
  this->write_(Register::PKTCTRL0, 0x32);
  ESP_LOGV(TAG, "Beginning TX sequence");
  if (this->gdo0_pin_ != nullptr) {
    // setup() only attaches the interrupt in packet mode. In async serial mode the pin may be
    // shared with remote_receiver, and detaching here would remove the receiver's interrupt.
    if (packet_mode) {
      this->gdo0_pin_->detach_interrupt();
    }
    this->gdo0_pin_->pin_mode(gpio::FLAG_OUTPUT);
  }
  // Transition through IDLE to bypass CCA (Clear Channel Assessment) which can
  // block TX entry when strobing from RX, and to ensure FS_AUTOCAL calibration
  this->enter_idle_();
  if (!this->enter_tx_()) {
    ESP_LOGW(TAG, "Failed to enter TX state!");
  }
}

void CC1101Component::begin_rx() {
  ESP_LOGV(TAG, "Beginning RX sequence");
  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  // Transition through IDLE to ensure FS_AUTOCAL calibration occurs
  this->enter_idle_();
  if (!this->enter_rx_()) {
    ESP_LOGW(TAG, "Failed to enter RX state!");
  }
}

void CC1101Component::reset() {
  this->strobe_(Command::RES);
  this->configure();
}

void CC1101Component::set_idle() {
  ESP_LOGV(TAG, "Setting IDLE state");
  this->enter_idle_();
}

bool CC1101Component::wait_for_state_(State target_state, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    this->read_(Register::MARCSTATE);
    State s = static_cast<State>(this->state_.MARC_STATE);
    if (s == target_state) {
      return true;
    }
    delayMicroseconds(100);
  }
  return false;
}

bool CC1101Component::enter_calibrated_(State target_state, Command cmd) {
  // The PLL must be recalibrated until PLL lock is achieved
  for (uint8_t retries = PLL_LOCK_RETRIES; retries > 0; retries--) {
    this->strobe_(cmd);
    if (!this->wait_for_state_(target_state)) {
      return false;
    }
    this->read_(Register::FSCAL1);
    if (this->state_.FSCAL1 != FSCAL1_PLL_NOT_LOCKED) {
      return true;
    }
    ESP_LOGW(TAG, "PLL lock failed, retrying calibration");
    this->enter_idle_();
  }
  ESP_LOGE(TAG, "PLL lock failed after retries");
  return false;
}

void CC1101Component::enter_idle_() {
  this->strobe_(Command::IDLE);
  this->wait_for_state_(State::IDLE);
}

bool CC1101Component::enter_rx_() { return this->enter_calibrated_(State::RX, Command::RX); }

bool CC1101Component::enter_tx_() { return this->enter_calibrated_(State::TX, Command::TX); }

uint8_t CC1101Component::strobe_(Command cmd) {
  uint8_t index = static_cast<uint8_t>(cmd);
  if (cmd < Command::RES || cmd > Command::NOP) {
    return 0xFF;
  }
  this->enable();
  uint8_t status_byte = this->transfer_byte(index);
  this->disable();
  return status_byte;
}

void CC1101Component::write_(Register reg) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index);
  this->write_array(&this->state_.regs()[index], 1);
  this->disable();
}

void CC1101Component::write_(Register reg, uint8_t value) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->state_.regs()[index] = value;
  this->write_(reg);
}

void CC1101Component::write_(Register reg, const uint8_t *buffer, size_t length) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_WRITE | BUS_BURST);
  this->write_array(buffer, length);
  this->disable();
}

void CC1101Component::read_(Register reg) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->state_.regs()[index] = this->transfer_byte(0);
  this->disable();
}

void CC1101Component::read_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = static_cast<uint8_t>(reg);
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->read_array(buffer, length);
  this->disable();
}

CC1101Error CC1101Component::transmit_packet(const std::vector<uint8_t> &packet) {
  if (this->state_.PKT_FORMAT != static_cast<uint8_t>(PacketFormat::PACKET_FORMAT_FIFO)) {
    return CC1101Error::PARAMS;
  }

  // Write packet
  this->enter_idle_();
  this->strobe_(Command::FTX);
  if (this->state_.LENGTH_CONFIG == static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE)) {
    this->write_(Register::FIFO, static_cast<uint8_t>(packet.size()));
  }
  this->write_(Register::FIFO, packet.data(), packet.size());

  // Calibrate PLL
  if (!this->enter_calibrated_(State::FSTXON, Command::FSTXON)) {
    ESP_LOGW(TAG, "PLL lock failed during TX");
    this->enter_idle_();
    this->enter_rx_();
    return CC1101Error::PLL_LOCK;
  }

  // Transmit packet
  this->strobe_(Command::TX);
  if (!this->wait_for_state_(State::IDLE, 1000)) {
    ESP_LOGW(TAG, "TX timeout");
    this->enter_idle_();
    this->enter_rx_();
    return CC1101Error::TIMEOUT;
  }

  // Return to rx
  this->enter_rx_();
  return CC1101Error::NONE;
}

// Setters
void CC1101Component::set_output_power(float value) {
  this->output_power_requested_ = value;
  int32_t freq = static_cast<int32_t>(this->state_.FREQ2 << 16 | this->state_.FREQ1 << 8 | this->state_.FREQ0) *
                 XTAL_FREQUENCY / (1 << 16);
  uint8_t a = 0xC0;
  if (freq >= 300000000 && freq <= 348000000) {
    a = PowerTableItem::find(PA_TABLE_315, sizeof(PA_TABLE_315) / sizeof(PA_TABLE_315[0]), value);
  } else if (freq >= 378000000 && freq <= 464000000) {
    a = PowerTableItem::find(PA_TABLE_433, sizeof(PA_TABLE_433) / sizeof(PA_TABLE_433[0]), value);
  } else if (freq >= 779000000 && freq < 900000000) {
    a = PowerTableItem::find(PA_TABLE_868, sizeof(PA_TABLE_868) / sizeof(PA_TABLE_868[0]), value);
  } else if (freq >= 900000000 && freq <= 928000000) {
    a = PowerTableItem::find(PA_TABLE_915, sizeof(PA_TABLE_915) / sizeof(PA_TABLE_915[0]), value);
  }

  if (static_cast<Modulation>(this->state_.MOD_FORMAT) == Modulation::MODULATION_ASK_OOK) {
    this->pa_table_[0] = 0;
    this->pa_table_[1] = a;
  } else {
    this->pa_table_[0] = a;
    this->pa_table_[1] = 0;
  }
  this->output_power_effective_ = value;
  if (this->initialized_) {
    this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
  }
}

void CC1101Component::set_rx_attenuation(RxAttenuation value) {
  this->state_.CLOSE_IN_RX = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FIFOTHR);
  }
}

void CC1101Component::set_dc_blocking_filter(bool value) {
  this->state_.DEM_DCFILT_OFF = value ? 0 : 1;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_frequency(float value) {
  int32_t freq = static_cast<int32_t>(value * (1 << 16) / XTAL_FREQUENCY);
  this->state_.FREQ2 = static_cast<uint8_t>(freq >> 16);
  this->state_.FREQ1 = static_cast<uint8_t>(freq >> 8);
  this->state_.FREQ0 = static_cast<uint8_t>(freq);
  if (this->initialized_) {
    this->enter_idle_();
    this->write_(Register::FREQ2);
    this->write_(Register::FREQ1);
    this->write_(Register::FREQ0);
    this->enter_rx_();
  }
}

void CC1101Component::set_if_frequency(float value) {
  this->state_.FREQ_IF = value * (1 << 10) / XTAL_FREQUENCY;
  if (this->initialized_) {
    this->write_(Register::FSCTRL1);
  }
}

void CC1101Component::set_filter_bandwidth(float value) {
  uint8_t e;
  uint32_t m;
  split_float(XTAL_FREQUENCY / (value * 8), 2, e, m);
  this->state_.CHANBW_E = e;
  this->state_.CHANBW_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG4);
  }
}

void CC1101Component::set_channel(uint8_t value) {
  this->state_.CHANNR = value;
  if (this->initialized_) {
    this->enter_idle_();
    this->write_(Register::CHANNR);
    this->enter_rx_();
  }
}

void CC1101Component::set_channel_spacing(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 18) / XTAL_FREQUENCY, 8, e, m);
  this->state_.CHANSPC_E = e;
  this->state_.CHANSPC_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG1);
    this->write_(Register::MDMCFG0);
  }
}

void CC1101Component::set_fsk_deviation(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 17) / XTAL_FREQUENCY, 3, e, m);
  this->state_.DEVIATION_E = e;
  this->state_.DEVIATION_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::DEVIATN);
  }
}

void CC1101Component::set_msk_deviation(uint8_t value) {
  this->state_.DEVIATION_E = 0;
  this->state_.DEVIATION_M = value - 1;
  if (this->initialized_) {
    this->write_(Register::DEVIATN);
  }
}

void CC1101Component::set_symbol_rate(float value) {
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 28) / XTAL_FREQUENCY, 8, e, m);
  this->state_.DRATE_E = e;
  this->state_.DRATE_M = static_cast<uint8_t>(m);
  if (this->initialized_) {
    this->write_(Register::MDMCFG4);
    this->write_(Register::MDMCFG3);
  }
}

void CC1101Component::set_sync_mode(SyncMode value) {
  this->state_.SYNC_MODE = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_carrier_sense_above_threshold(bool value) {
  this->state_.CARRIER_SENSE_ABOVE_THRESHOLD = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_modulation_type(Modulation value) {
  this->state_.MOD_FORMAT = static_cast<uint8_t>(value);
  this->state_.PA_POWER = value == Modulation::MODULATION_ASK_OOK ? 1 : 0;
  if (this->initialized_) {
    this->enter_idle_();
    this->set_output_power(this->output_power_requested_);
    this->write_(Register::MDMCFG2);
    this->write_(Register::FREND0);
    this->enter_rx_();
  }
}

void CC1101Component::set_manchester(bool value) {
  this->state_.MANCHESTER_EN = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::MDMCFG2);
  }
}

void CC1101Component::set_num_preamble(uint8_t value) {
  this->state_.NUM_PREAMBLE = value;
  if (this->initialized_) {
    this->write_(Register::MDMCFG1);
  }
}

void CC1101Component::set_sync1(uint8_t value) {
  this->state_.SYNC1 = value;
  if (this->initialized_) {
    this->write_(Register::SYNC1);
  }
}

void CC1101Component::set_sync0(uint8_t value) {
  this->state_.SYNC0 = value;
  if (this->initialized_) {
    this->write_(Register::SYNC0);
  }
}

void CC1101Component::set_magn_target(MagnTarget value) {
  this->state_.MAGN_TARGET = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_max_lna_gain(MaxLnaGain value) {
  this->state_.MAX_LNA_GAIN = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_max_dvga_gain(MaxDvgaGain value) {
  this->state_.MAX_DVGA_GAIN = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL2);
  }
}

void CC1101Component::set_carrier_sense_abs_thr(int8_t value) {
  this->state_.CARRIER_SENSE_ABS_THR = static_cast<uint8_t>(value & 0b1111);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_carrier_sense_rel_thr(CarrierSenseRelThr value) {
  this->state_.CARRIER_SENSE_REL_THR = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_lna_priority(bool value) {
  this->state_.AGC_LNA_PRIORITY = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::AGCCTRL1);
  }
}

void CC1101Component::set_filter_length_fsk_msk(FilterLengthFskMsk value) {
  this->state_.FILTER_LENGTH = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_filter_length_ask_ook(FilterLengthAskOok value) {
  this->state_.FILTER_LENGTH = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_freeze(Freeze value) {
  this->state_.AGC_FREEZE = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_wait_time(WaitTime value) {
  this->state_.WAIT_TIME = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_hyst_level(HystLevel value) {
  this->state_.HYST_LEVEL = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::AGCCTRL0);
  }
}

void CC1101Component::set_foc_bs_cs_gate(bool value) {
  this->state_.FOC_BS_CS_GATE = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_limit(FocLimit value) {
  this->state_.FOC_LIMIT = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_pre_k(FocPreK value) {
  this->state_.FOC_PRE_K = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_foc_post_k(FocPostK value) {
  this->state_.FOC_POST_K = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::FOCCFG);
  }
}

void CC1101Component::set_bs_limit(BsLimit value) {
  this->state_.BS_LIMIT = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_pre_ki(BsPreKi value) {
  this->state_.BS_PRE_KI = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_pre_kp(BsPreKp value) {
  this->state_.BS_PRE_KP = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_post_ki(BsPostKi value) {
  this->state_.BS_POST_KI = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_bs_post_kp(BsPostKp value) {
  this->state_.BS_POST_KP = static_cast<uint8_t>(value);
  if (this->initialized_) {
    this->write_(Register::BSCFG);
  }
}

void CC1101Component::set_packet_mode(bool value) {
  this->state_.PKT_FORMAT =
      static_cast<uint8_t>(value ? PacketFormat::PACKET_FORMAT_FIFO : PacketFormat::PACKET_FORMAT_ASYNC_SERIAL);
  if (value) {
    // Configure GDO0 to assert at the RX FIFO threshold or end of packet.
    this->state_.GDO0_CFG = 0x01;
    // FIFO_THR=7 means 32 bytes in RX FIFO (TI CC1101 datasheet, Table 36).
    // At 100 kbaud this leaves about 2.56 ms before the 64-byte FIFO fills.
    // Known Flowis frames (18/19 bytes in the rtl_433 reference capture; the
    // newer Type-1/Type-2 examples are also below 32 bytes) should finish
    // before this threshold and trigger on end-of-packet instead.
    // Longer candidates trigger early so loop() can log them before overflow.
    this->state_.FIFO_THR = 7;
    // Don't append status bytes to FIFO - we read from registers instead
    this->state_.APPEND_STATUS = 0;
  } else {
    // Configure GDO0 for serial data (async serial mode)
    this->state_.GDO0_CFG = 0x0D;
  }
  if (this->initialized_) {
    if (this->gdo0_pin_ != nullptr) {
      if (value) {
        this->gdo0_pin_->attach_interrupt(&CC1101Component::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
      } else {
        this->gdo0_pin_->detach_interrupt();
      }
    }
    this->write_(Register::PKTCTRL0);
    this->write_(Register::PKTCTRL1);
    this->write_(Register::IOCFG0);
    this->write_(Register::FIFOTHR);
  }
}

void CC1101Component::set_packet_length(uint8_t value) {
  if (value == 0) {
    this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_VARIABLE);
  } else {
    this->state_.LENGTH_CONFIG = static_cast<uint8_t>(LengthConfig::LENGTH_CONFIG_FIXED);
    this->state_.PKTLEN = value;
  }
  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
    this->write_(Register::PKTLEN);
  }
}

void CC1101Component::set_crc_enable(bool value) {
  this->state_.CRC_EN = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
  }
}

void CC1101Component::set_whitening(bool value) {
  this->state_.WHITE_DATA = value ? 1 : 0;
  if (this->initialized_) {
    this->write_(Register::PKTCTRL0);
  }
}

}  // namespace esphome::cc1101
