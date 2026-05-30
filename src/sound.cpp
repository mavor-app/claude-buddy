#include "sound.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include "driver/i2s.h"
#include "snd_alert.h"   // embedded 16kHz mono s16le clip from Downloads/sound.wav

// Self-contained ES8311 DAC bring-up, ported from Espressif's esp_codec_dev
// es8311 driver for this exact board: slave mode, external MCLK, 16-bit, 16 kHz,
// MCLK = 256*fs = 4.096 MHz (coeff row {4096000,16000,...}). The ESP is I2S
// master and emits MCLK on GPIO16, so install I2S first, then configure ES8311.

static const i2s_port_t PORT = I2S_NUM_0;
static const int        FS   = 16000;
static bool             ready = false;

static void w(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}
static uint8_t r(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((int)ES8311_ADDR, 1) != 1) return 0;
  return Wire.read();
}

static bool es8311_init() {
  // presence check
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) { Serial.println("[sound] ES8311 NOT found"); return false; }

  w(0x44, 0x08); w(0x44, 0x08);          // I2C noise immunity (driver writes twice)
  w(0x01, 0x30); w(0x02, 0x00); w(0x03, 0x10);
  w(0x16, 0x24); w(0x04, 0x10); w(0x05, 0x00);
  w(0x0B, 0x00); w(0x0C, 0x00); w(0x10, 0x1F); w(0x11, 0x7F);
  w(0x00, 0x80);
  w(0x00, r(0x00) & 0xBF);                // slave mode
  w(0x01, 0x3F);                          // use external MCLK, no invert
  w(0x06, r(0x06) & ~0x20);               // SCLK not inverted
  w(0x13, 0x10); w(0x1B, 0x0A); w(0x1C, 0x6A);
  w(0x44, 0x58);                          // internal ref (ADCL+DACR)

  // --- format: 16-bit, normal I2S ---
  w(0x09, r(0x09) | 0x0C); w(0x0A, r(0x0A) | 0x0C);   // 16-bit
  w(0x09, r(0x09) & 0xFC); w(0x0A, r(0x0A) & 0xFC);   // normal I2S

  // --- clock coeff for 4.096MHz / 16kHz (pre_div1 mult1 osr adc0x10 dac0x20 bclk4 lrcl0xff) ---
  w(0x02, r(0x02) & 0x07);                // pre_div-1=0, mult=0, use_mclk
  w(0x05, 0x00);                          // adc_div-1=0, dac_div-1=0
  w(0x03, (r(0x03) & 0x80) | 0x10);       // fs_mode 0, adc_osr 0x10
  w(0x04, (r(0x04) & 0x80) | 0x20);       // dac_osr 0x20
  w(0x07, r(0x07) & 0xC0);                // lrck_h 0
  w(0x08, 0xFF);                          // lrck_l 0xff
  w(0x06, (r(0x06) & 0xE0) | 0x03);       // bclk_div 4 -> 3

  // --- start, DAC mode ---
  w(0x00, 0x80); w(0x01, 0x3F);
  w(0x09, (r(0x09) & 0xBF) & ~0x40);      // DAC SDP out of reset
  w(0x0A, (r(0x0A) & 0xBF) | 0x40);
  w(0x17, 0xBF); w(0x0E, 0x02); w(0x12, 0x00); w(0x14, 0x1A);
  w(0x14, r(0x14) & ~0x40);               // no digital mic
  w(0x0D, 0x01); w(0x15, 0x40); w(0x37, 0x08); w(0x45, 0x00);
  w(0x31, 0x00);                          // DAC UN-mute (default may be muted)
  w(0x32, 0xC0);          // DAC ~-31dB hardware volume
  return true;
}

namespace sound {

bool begin() {
  // I2S master TX first, so MCLK runs on GPIO16 before ES8311 config.
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = FS;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = true;     // accurate audio clock — fractional MCLK from PLL_160M
                           // is too jittery for the ES8311 (silent codec)
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = FS * 256;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("[sound] i2s install failed"); return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_MCLK_PIN;
  pins.bck_io_num = I2S_BCLK_PIN;
  pins.ws_io_num  = I2S_WS_PIN;
  pins.data_out_num = I2S_DOUT_PIN;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;
  i2s_set_pin(PORT, &pins);
  i2s_zero_dma_buffer(PORT);
  delay(10);                              // let MCLK settle

  pinMode(PA_EN_PIN, OUTPUT);
  digitalWrite(PA_EN_PIN, LOW);           // amp off until we beep (saves power/heat)

  ready = es8311_init();
  Serial.printf("[sound] %s\n", ready ? "ready" : "codec init failed");
  if (ready) {
    digitalWrite(PA_EN_PIN, HIGH);       // keep amp ON — re-enabling per play left
                                         // it un-ramped, so later chirps were silent
    delay(150);
    alert();                             // boot confirmation = the clip
  }
  return ready;
}

void beep(int freqHz, int ms) {
  if (!ready) return;
  digitalWrite(PA_EN_PIN, HIGH);          // enable amp
  delay(2);

  const int N = 256;
  int16_t buf[N];
  uint32_t total = (uint32_t)FS * ms / 1000;
  float phase = 0, step = 2.0f * (float)M_PI * freqHz / FS;
  uint32_t done = 0; size_t wrote;
  while (done < total) {
    int n = (int)min((uint32_t)N, total - done);
    for (int i = 0; i < n; i++) {
      // gentle attack/release envelope so it's a soft "bip", not a click
      float env = 1.0f;
      uint32_t pos = done + i;
      if (pos < 200) env = pos / 200.0f;
      else if (pos > total - 200) env = (total - pos) / 200.0f;
      buf[i] = (int16_t)(sinf(phase) * 6000 * env);   // ~50% volume
      phase += step; if (phase > 2 * M_PI) phase -= 2 * M_PI;
    }
    i2s_write(PORT, buf, n * sizeof(int16_t), &wrote, portMAX_DELAY);
    done += n;
  }
  i2s_zero_dma_buffer(PORT);
  delay(4);
  digitalWrite(PA_EN_PIN, LOW);           // amp off again
}

// Play an embedded 16kHz mono s16le clip with a gain (0..1).
static void playClip(const uint8_t *bytes, size_t nbytes, float gain) {
  if (!ready) return;
  const int16_t *src = (const int16_t *)bytes;
  size_t n = nbytes / 2;
  digitalWrite(PA_EN_PIN, HIGH);          // idempotent; amp is kept on from begin()
  const int N = 256;
  int16_t buf[N];
  size_t off = 0, wrote;
  // Short silence lead-in so the very start of the clip isn't clipped.
  memset(buf, 0, sizeof(buf));
  for (int k = 0; k < 2; k++) i2s_write(PORT, buf, sizeof(buf), &wrote, portMAX_DELAY);
  while (off < n) {
    int m = (int)min((size_t)N, n - off);
    for (int i = 0; i < m; i++) buf[i] = (int16_t)(src[off + i] * gain);
    i2s_write(PORT, buf, m * sizeof(int16_t), &wrote, portMAX_DELAY);
    off += m;
  }
  i2s_zero_dma_buffer(PORT);              // trailing silence; leave PA on
}

void alert() { playClip(snd_alert_pcm, snd_alert_pcm_len, 0.4f); }  // custom chirp @50%

} // namespace sound
