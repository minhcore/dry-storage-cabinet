"""
UART logger + live plotter cho dry cabinet controller (STM32F411).

Thứ tự field gửi lên (khớp với đoạn code uart_send_int trong main loop):
  1. timestamp        (ms, HAL_GetTick() thô)
  2. raw_hum          (sht30.hum   *100 -> chia 100 để ra %)
  3. filtered_hum     (control.filtered_hum *100 -> chia 100)
  4. turn_on_thres    (*100 -> chia 100)
  5. turn_off_thres   (*100 -> chia 100)
  6. peltier          (0/1, thô)
  7. cold_fan        (0/1, thô)
  8. ntc_temp        (*100 -> chia 100)
  9. dew_point       (*100 -> chia 100)
  10. cycle_extreme_hum       (*100 -> chia 100)
  11. overshoot_on_learned    (*100 -> chia 100)
  12. overshoot_off_learned   (*100 -> chia 100)
  13. last_cycle_duration_ms  (ms, thô)
  14. phase_reversed          (0/1, thô)
  15. sht30_temp              (*100 -> chia 100)
  16. target_hum              (*100 -> chia 100)
  17. reversal_delta          (*100 -> chia 100)
  18. cold_fan_used_this_phase (0/1, thô)
  19. state_elapsed_ms        (ms, thô)

UART: USART1, 115200 baud, 8N1 — gửi mỗi 500ms (main.c: uart_tick interval).
Lưu ý: sensor + control_update chỉ chạy mỗi 1000ms, nên cứ 2 dòng UART liên
tiếp sẽ có 1 cặp giá trị giống hệt nhau (bình thường, không phải lỗi log).

Mỗi lần chạy script sẽ tự tạo 1 file CSV mới (tên gắn timestamp lúc chạy),
không ghi đè lên file cũ — chạy nhiều target khác nhau thì mở/tắt script
giữa mỗi lần đổi target để mỗi target có 1 file riêng, dễ so sánh sau này.

Cài đặt trước khi chạy:
    pip install pyserial matplotlib
"""

import csv
import time
from collections import deque
from datetime import datetime

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# ---- Cấu hình ----
COM_PORT = "COM4"
BAUD_RATE = 115200          # khớp USART1 trong main.c (huart1.Init.BaudRate)
WINDOW_SEC = 300             # số giây hiển thị gần nhất trên đồ thị (rolling window)
CSV_PATH = f"dry_cabinet_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

FIELDS = [
    "timestamp", "raw_hum", "filtered_hum",
    "turn_on_thres", "turn_off_thres", "peltier", "cold_fan",
    "ntc_temp", "dew_point", "cycle_extreme_hum",
    "overshoot_on_learned", "overshoot_off_learned",
    "last_cycle_duration_ms", "phase_reversed",
    "sht30_temp", "target_hum", "reversal_delta",
    "cold_fan_used_this_phase", "state_elapsed_ms",
]
# field nào cần chia 100 để ra đơn vị thực (% hoặc %/s hoặc °C)
SCALE_100 = {"raw_hum", "filtered_hum", "turn_on_thres", "turn_off_thres",
             "ntc_temp", "dew_point", "cycle_extreme_hum",
             "overshoot_on_learned", "overshoot_off_learned",
             "sht30_temp", "target_hum", "reversal_delta"}


def parse_line(line: str):
    parts = line.strip().split(",")
    if len(parts) != len(FIELDS):
        return None  # dòng lỗi/lẫn text khác, bỏ qua
    try:
        values = [int(p) for p in parts]
    except ValueError:
        return None
    row = dict(zip(FIELDS, values))
    for key in SCALE_100:
        row[key] = row[key] / 100.0
    return row


def main():
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    print(f"Đang mở {COM_PORT} @ {BAUD_RATE} baud, ghi log ra {CSV_PATH}")
    print("Nhấn Ctrl+C hoặc đóng cửa sổ đồ thị để dừng.\n")

    csv_file = open(CSV_PATH, "w", newline="")
    writer = csv.DictWriter(csv_file, fieldnames=FIELDS)
    writer.writeheader()

    # buffer cho plot (rolling window theo thời gian, không theo số sample cố định)
    buf = {k: deque() for k in FIELDS}
    session_start_ts = None   # mốc timestamp thiết bị lúc bắt đầu log, giữ cố định suốt phiên

    fig, (ax_hum, ax_reverse, ax_state, ax_temp, ax_learn) = plt.subplots(
        5, 1, sharex=True, figsize=(11, 10.5),
        gridspec_kw={"height_ratios": [3, 1.3, 1, 1.5, 1.3]},
    )
    fig.suptitle("Dry Cabinet — live debug (COM4)")

    def trim_window():
        if not buf["timestamp"]:
            return
        t_latest = buf["timestamp"][-1]
        t_min = t_latest - WINDOW_SEC * 1000
        while buf["timestamp"] and buf["timestamp"][0] < t_min:
            for k in FIELDS:
                buf[k].popleft()

    def update(_frame):
        nonlocal session_start_ts
        # đọc hết các dòng đang chờ trong buffer serial, không chỉ 1 dòng/frame
        while ser.in_waiting:
            raw_line = ser.readline().decode(errors="ignore")
            row = parse_line(raw_line)
            if row is None:
                continue
            if session_start_ts is None:
                session_start_ts = row["timestamp"]   # chốt mốc 0 đúng 1 lần, không đổi nữa
            writer.writerow(row)
            csv_file.flush()
            for k in FIELDS:
                buf[k].append(row[k])

        trim_window()
        if len(buf["timestamp"]) < 2:
            return

        t = [(ts - session_start_ts) / 1000.0 for ts in buf["timestamp"]]  # giây, mốc 0 = LÚC BẮT ĐẦU LOG, cố định

        for ax in (ax_hum, ax_reverse, ax_state, ax_temp, ax_learn):
            ax.cla()

        # 1) Độ ẩm: raw, filtered, 2 ngưỡng
        ax_hum.plot(t, buf["raw_hum"], color="lightgray", lw=1, label="raw_hum")
        ax_hum.plot(t, buf["filtered_hum"], color="tab:blue", lw=1.6, label="filtered_hum")
        ax_hum.plot(t, buf["turn_on_thres"], color="tab:green", lw=1, ls="--", label="turn_on_thres")
        ax_hum.plot(t, buf["turn_off_thres"], color="tab:red", lw=1, ls="--", label="turn_off_thres")
        ax_hum.plot(t, buf["target_hum"], color="tab:orange", lw=0.9, ls="-.", label="target_hum")
        ax_hum.plot(t, buf["cycle_extreme_hum"], color="black", lw=1, ls=":", label="cycle_extreme_hum")
        ax_hum.set_ylabel("%RH")
        ax_hum.legend(loc="upper right", fontsize=8, ncol=3)
        ax_hum.grid(alpha=0.3)

        # 2) Mức rời đỉnh/đáy dùng xác nhận đảo chiều
        ax_reverse.plot(t, buf["reversal_delta"], color="tab:purple", lw=1.2,
                        label="reversal_delta")
        ax_reverse.axhline(0.15, color="black", lw=0.8, ls="--",
                           label="confirm = 0.15%")
        ax_reverse.step(t, [0.25 if x else 0.0 for x in buf["phase_reversed"]],
                        where="post", color="tab:green", lw=1,
                        label="phase_reversed")
        ax_reverse.set_ylabel("%RH")
        ax_reverse.legend(loc="upper right", fontsize=8, ncol=3)
        ax_reverse.grid(alpha=0.3)

        # 3) Trạng thái Peltier / quạt (step plot, tách 2 mức để nhìn rõ)
        ax_state.step(t, buf["peltier"], where="post", color="tab:red", lw=1.5, label="peltier")
        ax_state.step(t, [c + 1.3 for c in buf["cold_fan"]], where="post",
                       color="tab:cyan", lw=1.5, label="cold_fan (+offset)")
        ax_state.step(t, [c + 2.6 for c in buf["cold_fan_used_this_phase"]],
                       where="post", color="tab:blue", lw=1.2,
                       label="fan_used_this_phase (+offset)")
        ax_state.set_ylabel("state")
        ax_state.set_yticks([])
        ax_state.legend(loc="upper right", fontsize=8, ncol=3)
        ax_state.grid(alpha=0.3)

        # 4) Nhiệt độ mặt lạnh vs dew point
        ax_temp.plot(t, buf["ntc_temp"], color="tab:orange", lw=1.3, label="ntc_temp")
        ax_temp.plot(t, buf["sht30_temp"], color="tab:blue", lw=1.1, label="sht30_temp")
        ax_temp.plot(t, buf["dew_point"], color="tab:brown", lw=1.3, ls="--", label="dew_point")
        ax_temp.set_ylabel("°C")
        ax_temp.set_xlabel("t (giây kể từ lúc bắt đầu log)")
        ax_temp.legend(loc="upper right", fontsize=8, ncol=3)
        ax_temp.grid(alpha=0.3)

        # 5) Margin đã học (EMA) — theo dõi hội tụ qua từng chu kỳ
        ax_learn.step(t, buf["overshoot_on_learned"], where="post", color="tab:green", lw=1.4, label="overshoot_on_learned")
        ax_learn.step(t, buf["overshoot_off_learned"], where="post", color="tab:red", lw=1.4, label="overshoot_off_learned")
        ax_learn.set_ylabel("%RH")
        ax_learn.legend(loc="upper right", fontsize=8, ncol=2)
        ax_learn.grid(alpha=0.3)

    ani = animation.FuncAnimation(fig, update, interval=500, cache_frame_data=False)
    try:
        plt.tight_layout()
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        csv_file.close()
        print(f"\nĐã đóng cổng, log lưu tại: {CSV_PATH}")


if __name__ == "__main__":
    main()
