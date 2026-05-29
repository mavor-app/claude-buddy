# Non-resetting serial reader (does NOT toggle DTR/RTS, so an active BLE
# connection survives). Usage: watch.py [port] [seconds]
import sys, time, serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1201"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 40.0

ser = serial.Serial()
ser.port = port
ser.baudrate = 115200
ser.timeout = 0.2
ser.dtr = False
ser.rts = False
ser.open()

end = time.time() + secs
while time.time() < end:
    data = ser.readline()
    if data:
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()
ser.close()
