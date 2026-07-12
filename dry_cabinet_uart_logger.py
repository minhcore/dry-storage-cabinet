#!/usr/bin/env python3
"""
UART logger + realtime plotter for the dry cabinet (tu chong am) project - STM32F411.

Expected UART line format (see the accompanying main.c change):
    <mcu_tick_ms>,<temp_x100>,<hum_x100>\r\n
Example:
    123456,2453,6120   ->  tick=123456 ms, T=24.53 C, RH=61.20 %

Each run produces two files in the current directory:
    dry_cabinet_log_<timestamp>.csv  - clean parsed data, ready for pandas/Excel
                                        columns: pc_time, mcu_tick_ms, elapsed_s, temp_c, hum_pct
    dry_cabinet_raw_<timestamp>.txt  - every raw line received, unmodified (safety net)

Setup:
    pip install pyserial matplotlib

Usage:
    python dry_cabinet_uart_logger.py --list              # find your port name
    python dry_cabinet_uart_logger.py --port COM5          # Windows example
    python dry_cabinet_uart_logger.py --port /dev/ttyUSB0  # Linux example

Close the plot window (or Ctrl+C in the terminal) to stop. Both files are
flushed after every line, so nothing is lost even if you kill the script.
"""

import argparse
import csv
import sys
from datetime import datetime

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation


def parse_line(line):
    """Parse one raw UART line into (tick_ms, temp_c, hum_pct), or None if malformed."""
    parts = line.strip().split(",")
    if len(parts) != 3:
        return None
    try:
        tick_ms = int(parts[0])
        temp_c = int(parts[1]) / 100.0
        hum_pct = int(parts[2]) / 100.0
    except ValueError:
        return None
    return tick_ms, temp_c, hum_pct


def print_available_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device}  -  {p.description}")


def parse_args():
    p = argparse.ArgumentParser(description="Log + realtime-plot UART data from the dry cabinet MCU")
    p.add_argument("--port", help="e.g. COM5 (Windows) or /dev/ttyUSB0 (Linux)")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--out", default=None, help="CSV filename (default: auto timestamp)")
    p.add_argument("--list", action="store_true", help="List available serial ports and exit")
    return p.parse_args()


def main():
    args = parse_args()

    if args.list:
        print_available_ports()
        return

    if not args.port:
        print("Error: --port is required. Run with --list to see available ports.")
        sys.exit(1)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Could not open {args.port}: {e}")
        sys.exit(1)

    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = args.out or f"dry_cabinet_log_{ts}.csv"
    raw_path = f"dry_cabinet_raw_{ts}.txt"

    csv_f = open(csv_path, "w", newline="")
    csv_w = csv.writer(csv_f)
    csv_w.writerow(["pc_time", "mcu_tick_ms", "elapsed_s", "temp_c", "hum_pct"])

    raw_f = open(raw_path, "w")

    t0 = None
    xs, temps, hums = [], [], []

    fig, (ax_t, ax_h) = plt.subplots(2, 1, sharex=True, figsize=(9, 6))
    line_t, = ax_t.plot([], [], color="tab:red", lw=1.3)
    line_h, = ax_h.plot([], [], color="tab:blue", lw=1.3)
    ax_t.set_ylabel("Temperature (C)")
    ax_h.set_ylabel("Humidity (%RH)")
    ax_h.set_xlabel("Elapsed time (min)")
    ax_t.grid(alpha=0.3)
    ax_h.grid(alpha=0.3)
    fig.suptitle(f"Dry cabinet monitor - {args.port}")

    def handle_line(raw_line):
        nonlocal t0
        raw_f.write(raw_line.rstrip("\r\n") + "\n")
        raw_f.flush()

        parsed = parse_line(raw_line)
        if parsed is None:
            return
        tick_ms, temp_c, hum_pct = parsed

        if t0 is None:
            t0 = tick_ms
        elapsed_s = (tick_ms - t0) / 1000.0

        csv_w.writerow([datetime.now().isoformat(), tick_ms, elapsed_s, temp_c, hum_pct])
        csv_f.flush()

        xs.append(elapsed_s / 60.0)
        temps.append(temp_c)
        hums.append(hum_pct)
        print(f"t={elapsed_s:7.1f}s  T={temp_c:5.2f}C  RH={hum_pct:5.2f}%")

    def update(_frame):
        try:
            while ser.in_waiting:
                raw = ser.readline().decode("utf-8", errors="replace")
                if raw:
                    handle_line(raw)
        except serial.SerialException:
            print("\nSerial port disconnected - stopping.")
            ani.event_source.stop()

        if xs:
            line_t.set_data(xs, temps)
            line_h.set_data(xs, hums)
            for ax, ys in ((ax_t, temps), (ax_h, hums)):
                lo, hi = min(ys), max(ys)
                pad = max((hi - lo) * 0.15, 0.5)
                ax.set_xlim(0, max(xs[-1], 0.1))
                ax.set_ylim(lo - pad, hi + pad)
        return line_t, line_h

    # keep a reference to `ani`, or the animation gets garbage-collected and stops
    ani = animation.FuncAnimation(fig, update, interval=200, cache_frame_data=False)

    try:
        plt.tight_layout()
        plt.show()
    finally:
        ser.close()
        csv_f.close()
        raw_f.close()
        print(f"\nSaved: {csv_path} (parsed data), {raw_path} (raw backup)")


if __name__ == "__main__":
    main()
