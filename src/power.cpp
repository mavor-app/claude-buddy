#include "power.h"
#include "config.h"
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static XPowersPMU pmu;
static bool ready = false;

namespace power {

bool begin() {
  // The shared I2C bus is brought up here (first device to need it).
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);

  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[power] AXP2101 NOT found — panel will stay dark!");
    return false;
  }
  Serial.println("[power] AXP2101 online");

  // --- Panel power rail (DSI_PWR_EN) ---------------------------------------
  // VERIFY against the Waveshare ESP32-S3-Touch-AMOLED-2.06 demo which rail
  // feeds the CO5300. On the Waveshare AMOLED boards the AMOLED logic/IO is
  // typically on BLDO1 (~3.3V) with the I2C peripherals on ALDO1. Enabling a
  // small superset here is harmless; trim to match the demo once confirmed.
  pmu.setBLDO1Voltage(3300);  pmu.enableBLDO1();   // AMOLED panel
  pmu.setALDO1Voltage(3300);  pmu.enableALDO1();   // touch / sensors
  pmu.setALDO3Voltage(3300);  pmu.enableALDO3();
  delay(50);

  // --- Battery / charger sane defaults -------------------------------------
  pmu.setSysPowerDownVoltage(2700);
  pmu.enableBattDetection();
  pmu.enableVbusVoltageMeasure();
  pmu.enableBattVoltageMeasure();
  pmu.enableSystemVoltageMeasure();

  ready = true;
  return true;
}

BatteryInfo read() {
  BatteryInfo b;
  if (!ready) return b;
  b.usb     = pmu.isVbusIn();
  b.present = pmu.isBatteryConnect();
  if (b.present) {
    b.pct = pmu.getBatteryPercent();
    b.mV  = pmu.getBattVoltage();
    // AXP2101 exposes no battery-current ADC (unlike AXP192). Spec only uses
    // mA's sign to mean "charging" (negative), so encode that from the charger
    // state: -1 while charging, 0 otherwise.
    b.mA = pmu.isCharging() ? -1 : 0;
  }
  return b;
}

} // namespace power
