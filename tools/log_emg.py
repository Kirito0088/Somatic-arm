"""Log the Somatic Arm's serial output to CSV.

The firmware prints lines like `emg:512 close:600 open:550` at 115200 baud.
Each line becomes one CSV row: t_ms (time since the port opened) plus every
key:value pair on the line.

Usage: py tools\\log_emg.py COM5 60 session1.csv
Needs pyserial: py -m pip install pyserial
Close the Arduino IDE's Serial Monitor/Plotter first: only one program can
hold the port. Opening the port resets the Uno, so calibration runs again.
"""
import csv
import sys
import time


def parse_line(line):
    """`emg:512 close:600` -> {"emg": "512", "close": "600"}, or None for partial/garbage lines.

    The first line after the port opens is often cut off (`lose:600 open:550`),
    so a line must start with the emg field to count.
    """
    pairs = line.split()
    if not pairs or not pairs[0].startswith("emg:") or any(":" not in p for p in pairs):
        return None
    return dict(p.split(":", 1) for p in pairs)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    port, seconds, out_path = sys.argv[1], float(sys.argv[2]), sys.argv[3]

    import serial  # imported here so parse_line can be tested without pyserial

    rows = 0
    with serial.Serial(port, 115200, timeout=1) as ser, open(out_path, "w", newline="") as f:
        writer = None
        start = time.time()
        while time.time() - start < seconds:
            row = parse_line(ser.readline().decode(errors="ignore").strip())
            if row is None:
                continue
            row = {"t_ms": int((time.time() - start) * 1000), **row}
            if writer is None:
                writer = csv.DictWriter(f, fieldnames=list(row))
                writer.writeheader()
            if list(row) == writer.fieldnames:
                writer.writerow(row)
                rows += 1
    print(f"Wrote {rows} rows to {out_path}")


if __name__ == "__main__":
    main()
