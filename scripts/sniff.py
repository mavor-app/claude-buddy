# Non-interactive serial reader: reset the board, then capture boot log.
import sys, time, serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1201"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0

ser = serial.Serial(port, 115200, timeout=0.2)
# Pulse reset (ESP32-S3 USB-Serial-JTAG honors RTS->EN).
ser.setDTR(False); ser.setRTS(True); time.sleep(0.15)
ser.setRTS(False); time.sleep(0.05)

end = time.time() + secs
while time.time() < end:
    data = ser.readline()
    if data:
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()
ser.close()
