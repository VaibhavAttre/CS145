import serial
import re
import time

PORT = "COM3"
PC_BAUD = 115200

pattern = re.compile(rb"BAUDSCAN BRR=(\d+)")

print(f"Opening {PORT} at {PC_BAUD} baud...")
print("Flash the STM32 baud scanner firmware first.")
print("Watching for readable BAUDSCAN messages...\n")

try:
    ser = serial.Serial(
        port=PORT,
        baudrate=PC_BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.5
    )
except serial.SerialException as e:
    print("Could not open serial port.")
    print(e)
    print("\nMake sure PuTTY / TeraTerm / STM32CubeIDE serial monitor is closed.")
    raise SystemExit

seen_counts = {}

while True:
    data = ser.read(512)

    if not data:
        continue

    # Print readable-ish chunks for debugging
    try:
        text = data.decode("ascii", errors="ignore")
        if "BAUDSCAN" in text or "BRR" in text:
            print(text, end="")
    except Exception:
        pass

    matches = pattern.findall(data)

    for match in matches:
        brr = int(match.decode("ascii"))
        seen_counts[brr] = seen_counts.get(brr, 0) + 1

        print("\n\nFOUND READABLE UART MESSAGE")
        print("---------------------------")
        print(f"PC baud rate: {PC_BAUD}")
        print(f"STM32 BRR:    {brr}")
        print(f"Seen count:   {seen_counts[brr]}")

        if seen_counts[brr] >= 2:
            print("\nMost likely correct setting:")
            print(f"*USART3_BRR = {brr};")
            print("\nYou can stop this script with Ctrl+C.")