// config.h — board pins, BLE UUIDs, and tunables for Claude Hardware Buddy.
//
// Pins come from the Waveshare ESP32-S3-Touch-AMOLED-2.06 pinout. If the
// board-support package "Mylibrary" defines equivalent macros, prefer those:
// it is the source of truth for this exact board revision.
#pragma once
#include <Arduino.h>

#if __has_include(<Mylibrary.h>)
  #include <Mylibrary.h>   // pulls in board pin macros if present
#endif

// ---------------------------------------------------------------------------
// Display — CO5300 AMOLED over QSPI
// ---------------------------------------------------------------------------
#ifndef LCD_SDIO0
  #define LCD_SDIO0 4
  #define LCD_SDIO1 5
  #define LCD_SDIO2 6
  #define LCD_SDIO3 7
  #define LCD_SCLK  11
  #define LCD_CS    12
  #define LCD_RST   8
  #define LCD_TE    13
#endif

#define LCD_WIDTH   410
#define LCD_HEIGHT  502
#define LCD_BRIGHTNESS 110   // 0..255 active brightness (lower = cooler/less power)
// Idle dimming to cut heat/power on this always-on AMOLED.
#define BRIGHT_ACTIVE   110
#define BRIGHT_DIM       28
#define IDLE_DIM_MS    30000UL   // dim after 30s with no touch / prompt
#define IDLE_OFF_MS   120000UL   // panel ~off after 2 min

// ---------------------------------------------------------------------------
// Shared I2C bus (touch / PMIC / IMU / RTC all live here)
// ---------------------------------------------------------------------------
#ifndef IIC_SDA
  #define IIC_SDA 15
  #define IIC_SCL 14
#endif

// Touch FT3168
#ifndef TP_RST
  #define TP_RST 9
  #define TP_INT 38
#endif
// Default 7-bit address for FT3168 is 0x38. // VERIFY against Waveshare demo.
#ifndef FT3168_ADDR
  #define FT3168_ADDR 0x38
#endif

// IMU / RTC interrupt lines
#ifndef IMU_INT
  #define IMU_INT 21
#endif
#ifndef RTC_INT
  #define RTC_INT 39
#endif

// ---------------------------------------------------------------------------
// Buttons (active-low; both have external/internal pull-ups)
// ---------------------------------------------------------------------------
#ifndef BTN_APPROVE
  #define BTN_APPROVE 0    // BOOT  -> Approve / Confirm / Next
  #define BTN_DENY    10   // PWR   -> Deny / Back
#endif

// Audio: ES8311 codec (I2C) + I2S + power amp. Playback data -> DSDIN/GPIO40
// (GPIO42/ASDOUT is the mic ADC output, not playback).
#ifndef I2S_MCLK_PIN
  #define I2S_MCLK_PIN 16
  #define I2S_BCLK_PIN 41   // I2S_SCLK
  #define I2S_WS_PIN   45   // I2S_LRCK
  #define I2S_DOUT_PIN 40   // I2S_DSDIN -> ES8311 DAC
  #define PA_EN_PIN    46   // PA_CTRL
#endif
#define ES8311_ADDR   0x18

// ---------------------------------------------------------------------------
// BLE — Nordic UART Service
// ---------------------------------------------------------------------------
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // desktop -> device (write)
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // device -> desktop (notify)

#define BLE_NAME_PREFIX  "Claude"     // advertised name MUST start with this
#define HEARTBEAT_TIMEOUT_MS 30000UL  // no snapshot for ~30s => treat as disconnected

// LE Secure Connections bonding (milestone 7). Temporarily OFF: prove the
// plaintext data path first (the desktop supports sec:false devices). Flip to
// 1 to re-enable pairing once the pipeline is verified end-to-end.
#define BLE_REQUIRE_ENCRYPTION 0

// ---------------------------------------------------------------------------
// Protocol limits
// ---------------------------------------------------------------------------
#define MAX_LINE_BYTES   8192   // a single \n-delimited JSON line ceiling
#define MAX_ENTRIES      8      // transcript lines kept for display

// Rolling-5h output-token bar: fill is (5h usage / this cap). Placeholder —
// the real plan limit isn't in the protocol. Tune to your observed ceiling.
#define USAGE_5H_CAP     200000UL
#define TURN_EVT_MAX     4096   // turn events larger than this are dropped (spec)
