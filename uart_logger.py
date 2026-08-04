#!/usr/bin/env python3
"""Capture and live-plot the dry-cabinet UART CSV stream.

Expected MCU field order:
timestamp,raw_hum,target_hum,turn_on_thres,turn_off_thres,peltier,
cold_fan,ntc_temp,dew_point,state_elapsed_ms,profile,
condensing_started,fan_boost_allowed,sht30_temp

Default connection: COM4, 115200 baud.

Example:
    python uart_logger.py --output log_50.csv

Use --no-plot when only CSV capture is required.
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError as exc:
    raise SystemExit(
        "Missing pyserial. Install it with: pip install pyserial"
    ) from exc


FIELDS = [
    "timestamp",
    "raw_hum",
    "target_hum",
    "turn_on_thres",
    "turn_off_thres",
    "peltier",
    "cold_fan",
    "ntc_temp",
    "dew_point",
    "state_elapsed_ms",
    "profile",
    "condensing_started",
    "fan_boost_allowed",
    "sht30_temp",
]

INTEGER_FIELDS = {
    "timestamp",
    "peltier",
    "cold_fan",
    "state_elapsed_ms",
    "profile",
    "condensing_started",
    "fan_boost_allowed",
}

SCALE_100_FIELDS = {
    "raw_hum",
    "target_hum",
    "turn_on_thres",
    "turn_off_thres",
    "ntc_temp",
    "dew_point",
    "sht30_temp",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Log and live-plot dry-cabinet UART CSV data."
    )
    parser.add_argument(
        "--port",
        default="COM4",
        help="Serial port (default: COM4).",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="UART baud rate (default: 115200).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output CSV path (default: timestamped dry_cabinet_log_*.csv).",
    )
    parser.add_argument(
        "--no-plot",
        action="store_true",
        help="Capture CSV without opening a live plot.",
    )
    return parser.parse_args()


def parse_uart_line(line: str) -> dict[str, int | float] | None:
    line = line.strip()

    if not line:
        return None

    # Accept, but do not store, an optional header emitted by the MCU.
    if line.lower().startswith("timestamp,"):
        return None

    parts = [part.strip() for part in line.split(",")]

    if len(parts) != len(FIELDS):
        return None

    row: dict[str, int | float] = {}

    try:
        for field, value in zip(FIELDS, parts):
            raw_value = int(value)
            if field in SCALE_100_FIELDS:
                row[field] = raw_value / 100.0
            elif field in INTEGER_FIELDS:
                row[field] = raw_value
            else:
                row[field] = float(value)
    except ValueError:
        return None

    return row


class LivePlot:
    def __init__(self) -> None:
        try:
            import matplotlib.pyplot as plt
        except ImportError as exc:
            raise SystemExit(
                "Missing matplotlib. Install it with: pip install matplotlib"
            ) from exc

        self.plt = plt
        self.last_timestamp: int | None = None

        # Lists intentionally retain the whole session: this is a full-history
        # plot, not a rolling window.
        self.data = {field: [] for field in FIELDS}
        self.time_s: list[float] = []

        plt.ion()
        self.figure, (self.ax_hum, self.ax_temp, self.ax_state) = plt.subplots(
            3,
            1,
            figsize=(12, 8),
            sharex=True,
            gridspec_kw={"height_ratios": [2.0, 1.4, 1.0]},
        )
        self.figure.suptitle("Dry cabinet control log")

        (self.line_hum,) = self.ax_hum.plot([], [], label="Raw RH", linewidth=1.4)
        (self.line_target,) = self.ax_hum.plot([], [], label="Target", linestyle="--")
        (self.line_lower_3,) = self.ax_hum.plot(
            [], [], color="tab:green", label="Target - 3%", linewidth=2.4
        )
        (self.line_upper_3,) = self.ax_hum.plot(
            [], [], color="tab:red", label="Target + 3%", linewidth=2.4
        )
        (self.line_on,) = self.ax_hum.plot([], [], label="Turn ON", linestyle=":")
        (self.line_off,) = self.ax_hum.plot([], [], label="Turn OFF", linestyle=":")

        (self.line_ntc,) = self.ax_temp.plot([], [], label="Cold plate NTC")
        (self.line_dew,) = self.ax_temp.plot([], [], label="Dew point", linestyle="--")
        (self.line_air,) = self.ax_temp.plot([], [], label="Cabinet temp", alpha=0.8)

        (self.line_peltier,) = self.ax_state.step([], [], where="post", label="Peltier")
        (self.line_fan,) = self.ax_state.step([], [], where="post", label="Cold fan")

        self.ax_hum.set_ylabel("RH (%)")
        self.ax_temp.set_ylabel("Temperature (°C)")
        self.ax_state.set_ylabel("State")
        self.ax_state.set_xlabel("Time (s)")
        self.ax_state.set_ylim(-0.1, 1.2)
        self.ax_state.set_yticks([0, 1])

        for axis in (self.ax_hum, self.ax_temp, self.ax_state):
            axis.grid(True, alpha=0.25)
            axis.legend(loc="upper right")

        self.figure.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))

    def clear(self) -> None:
        for values in self.data.values():
            values.clear()
        self.time_s.clear()
        self.last_timestamp = None

    def append(self, row: dict[str, int | float]) -> None:
        timestamp = int(row["timestamp"])

        if self.last_timestamp is None:
            elapsed_s = 0.0
        elif timestamp >= self.last_timestamp:
            elapsed_s = self.time_s[-1] + (timestamp - self.last_timestamp) / 1000.0
        elif self.last_timestamp > 0xF0000000:
            # HAL_GetTick() wrapped after roughly 49.7 days.
            delta_ms = (0x100000000 - self.last_timestamp) + timestamp
            elapsed_s = self.time_s[-1] + delta_ms / 1000.0
        else:
            # MCU reset: preserve the old plot and continue the session timeline.
            elapsed_s = self.time_s[-1] + 0.5

        self.last_timestamp = timestamp
        self.time_s.append(elapsed_s)

        for field in FIELDS:
            self.data[field].append(row[field])

    def redraw(self) -> None:
        if not self.time_s:
            return

        x = list(self.time_s)

        self.line_hum.set_data(x, self.data["raw_hum"])
        self.line_target.set_data(x, self.data["target_hum"])
        self.line_lower_3.set_data(
            x, [target - 3.0 for target in self.data["target_hum"]]
        )
        self.line_upper_3.set_data(
            x, [target + 3.0 for target in self.data["target_hum"]]
        )
        self.line_on.set_data(x, self.data["turn_on_thres"])
        self.line_off.set_data(x, self.data["turn_off_thres"])

        self.line_ntc.set_data(x, self.data["ntc_temp"])
        self.line_dew.set_data(x, self.data["dew_point"])
        self.line_air.set_data(x, self.data["sht30_temp"])

        self.line_peltier.set_data(x, self.data["peltier"])
        self.line_fan.set_data(x, self.data["cold_fan"])

        for axis in (self.ax_hum, self.ax_temp):
            axis.relim()
            axis.autoscale_view()

        # Always show the complete elapsed session from t=0 to the newest row.
        self.ax_state.set_xlim(0.0, max(x[-1], 1.0))
        self.figure.canvas.draw_idle()
        self.figure.canvas.flush_events()
        self.plt.pause(0.001)


def main() -> int:
    args = parse_args()
    if args.output is None:
        args.output = Path(
            f"dry_cabinet_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)

    live_plot = None if args.no_plot else LivePlot()

    invalid_lines = 0
    rows_written = 0
    last_console_update = 0.0

    print(f"Opening {args.port} at {args.baud} baud")
    print(f"Writing {args.output}")
    print("Press Ctrl+C to stop safely.")

    try:
        with serial.Serial(args.port, args.baud, timeout=1.0) as uart:
            # Discard a possible partial line that was already in the OS buffer.
            uart.reset_input_buffer()

            with args.output.open("w", newline="", encoding="utf-8") as csv_file:
                writer = csv.DictWriter(csv_file, fieldnames=FIELDS)
                writer.writeheader()
                csv_file.flush()

                while True:
                    raw_line = uart.readline()

                    if not raw_line:
                        if live_plot is not None:
                            live_plot.redraw()
                        continue

                    line = raw_line.decode("utf-8", errors="replace")
                    row = parse_uart_line(line)

                    if row is None:
                        if line.strip() and not line.lower().startswith("timestamp,"):
                            invalid_lines += 1
                            if invalid_lines <= 5 or invalid_lines % 100 == 0:
                                print(
                                    f"Ignored malformed UART line #{invalid_lines}: "
                                    f"{line.strip()}",
                                    file=sys.stderr,
                                )
                        continue

                    writer.writerow(row)
                    csv_file.flush()
                    rows_written += 1

                    if live_plot is not None:
                        live_plot.append(row)
                        live_plot.redraw()

                    now = time.monotonic()
                    if now - last_console_update >= 1.0:
                        last_console_update = now
                        print(
                            "t={:>8d} ms  RH={:>6.2f}%  target={:>5.1f}%  "
                            "P={}  fan={}  NTC={:>6.2f}°C  profile={}".format(
                                int(row["timestamp"]),
                                float(row["raw_hum"]),
                                float(row["target_hum"]),
                                int(row["peltier"]),
                                int(row["cold_fan"]),
                                float(row["ntc_temp"]),
                                int(row["profile"]),
                            )
                        )

    except KeyboardInterrupt:
        print(
            f"\nStopped. Saved {rows_written} rows to {args.output}. "
            f"Ignored {invalid_lines} malformed lines."
        )
        return 0
    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
