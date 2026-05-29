# Inject a test snapshot (with a pending permission prompt) over USB serial,
# then watch for the device's reaction + your button press. Does NOT reset.
import sys, time, serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1201"
watch = float(sys.argv[2]) if len(sys.argv) > 2 else 25.0

SNAP = (
    '{"total":3,"running":1,"waiting":2,"msg":"approve: Bash",'
    '"entries":["10:42 git push","10:41 yarn test","10:39 reading file"],'
    '"tokens":184502,"tokens_today":31200,'
    '"prompt":{"id":"req_test_001","tool":"Bash","hint":"rm -rf /tmp/foo"}}\n'
)

ser = serial.Serial()
ser.port = port; ser.baudrate = 115200; ser.timeout = 0.2
ser.dtr = False; ser.rts = False
ser.open()

time.sleep(0.5)
ser.write(SNAP.encode("utf-8"))
ser.flush()
print(">>> injected snapshot with prompt req_test_001 (tool=Bash)")
print(">>> press APPROVE (BOOT / green) or DENY (PWR / red) on the device...\n")

end = time.time() + watch
while time.time() < end:
    data = ser.readline()
    if data:
        sys.stdout.write(data.decode("utf-8", "replace")); sys.stdout.flush()
ser.close()
