import csv
import os
import re
import threading
from collections import deque
from datetime import datetime
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import time

import serial
import serial.tools.list_ports


class STM32SensorMonitor:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("STM32 Sensor-Monitor")
        self.root.geometry("620x400")
        self.root.minsize(560, 340)

        self.ser = None
        self.running = False
        self.rx_thread = None

        # Ringpuffer für die letzten 120 Messpunkte (2 Minuten bei 1 Hz)
        self.max_points = 600
        self.data = {
            "stts_t": deque(maxlen=self.max_points),
            "hts_t":  deque(maxlen=self.max_points),
            "lps_t":  deque(maxlen=self.max_points),
            "hts_rh": deque(maxlen=self.max_points),
        }

        self.temp_cfg = [
            ("stts_t", "STTS (°C)", "#e63946"),  # Rot
            ("hts_t",  "HTS (°C)",  "#f4a261"),  # Orange
            ("lps_t",  "LPS (°C)",  "#2a9d8f"),  # Türkis
        ]
        self.rh_color = "#0077b6"  # Blau

        self.pattern = re.compile(
            r"STTS:\s*([-+]?\d+(?:\.\d+)?)\s*C\s*\|\s*"
            r"HTS:\s*([-+]?\d+(?:\.\d+)?)\s*C,\s*([-+]?\d+(?:\.\d+)?)\s*%\s*\|\s*"
            r"LPS:\s*([-+]?\d+(?:\.\d+)?)\s*C,\s*([-+]?\d+(?:\.\d+)?)\s*hPa"
        )

        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Getrennt")
        self.log_enabled_var = tk.BooleanVar(value=False)
        self.csv_path_var = tk.StringVar(value=r"C:\Temp\raumtemperatur.csv")
        self.pressure_var = tk.StringVar(value="Druck: -- hPa")
        self.val_labels = {}

        self.log_interval_var = tk.IntVar(value=10)      # Sekunden
        self.diagram_minutes_var = tk.IntVar(value=10)   # Minuten
        self.last_log_time = 0

        self._build_ui()
        self._refresh_ports()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self):
        # --- 1. Obere Steuerleiste ---
        top_frame = ttk.Frame(self.root, padding=(8, 6))
        top_frame.pack(fill=tk.X)

        ttk.Label(top_frame, text="COM-Port:").pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(top_frame, textvariable=self.port_var, width=12, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=(4, 4))

        ttk.Button(top_frame, text="↻", width=3, command=self._refresh_ports).pack(side=tk.LEFT, padx=(0, 6))

        self.conn_btn = ttk.Button(top_frame, text="Connect", width=11, command=self._toggle_connection)
        self.conn_btn.pack(side=tk.LEFT)

        ttk.Label(top_frame, textvariable=self.status_var, foreground="#555555").pack(side=tk.RIGHT)

        # --- 2. Aktuelle Werte & separates Luftdruck-Feld ---
        val_frame = ttk.Frame(self.root, padding=(8, 2))
        val_frame.pack(fill=tk.X)

        for key, label, color in self.temp_cfg:
            item = ttk.Frame(val_frame)
            item.pack(side=tk.LEFT, padx=(0, 10))
            tk.Label(item, text="■", fg=color, font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT)
            lbl = ttk.Label(item, text=f"{label.split(' ')[0]}: --", font=("Segoe UI", 8))
            lbl.pack(side=tk.LEFT, padx=(2, 0))
            self.val_labels[key] = (label, lbl)

        # Feuchte-Wert
        rh_item = ttk.Frame(val_frame)
        rh_item.pack(side=tk.LEFT, padx=(0, 10))
        tk.Label(rh_item, text="■", fg=self.rh_color, font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT)
        rh_lbl = ttk.Label(rh_item, text="Feuchte: --", font=("Segoe UI", 8))
        rh_lbl.pack(side=tk.LEFT, padx=(2, 0))
        self.val_labels["hts_rh"] = ("Feuchte (%)", rh_lbl)

        # Separates Infofeld für Luftdruck rechts
        press_box = ttk.Label(
            val_frame,
            textvariable=self.pressure_var,
            font=("Segoe UI", 8, "bold"),
            background="#f0f2f5",
            foreground="#333333",
            padding=(6, 2),
            relief="solid",
            borderwidth=1
        )
        press_box.pack(side=tk.RIGHT)

        # --- 3. Einstellungen ---
        settings_frame = ttk.Frame(self.root, padding=(8, 2))
        settings_frame.pack(fill=tk.X)

        ttk.Label(
            settings_frame,
            text="Verlauf (sec):"
        ).pack(side=tk.LEFT, padx=(10, 4))

        history_combo = ttk.Combobox(
            settings_frame,
            textvariable=self.diagram_minutes_var,
            state="readonly",
            width=5,
            values=(2, 5, 10, 30, 60)
        )

        history_combo.pack(side=tk.LEFT)

        history_combo.bind(
            "<<ComboboxSelected>>",
            lambda e: self._change_history()
        )

        # --- 3. Diagramm-Bereich (oben: Temperatur [2/3], unten: Feuchte [1/3]) ---
        canvas_frame = ttk.Frame(self.root, padding=(8, 4))
        canvas_frame.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(canvas_frame, bg="#ffffff", highlightthickness=1, highlightbackground="#cccccc")
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda e: self._redraw_charts())

        # --- 4. Untere CSV-Leiste ---
        bot_frame = ttk.Frame(self.root, padding=(8, 6))
        bot_frame.pack(fill=tk.X)

        ttk.Checkbutton(
            bot_frame,
            text="CSV-Log:",
            variable=self.log_enabled_var
        ).pack(side=tk.LEFT)

        ttk.Label(
            bot_frame,
            text="Intervall (sec):"
        ).pack(side=tk.LEFT, padx=(10, 2))

        ttk.Combobox(
            bot_frame,
            textvariable=self.log_interval_var,
            state="readonly",
            width=5,
            values=(1, 5, 10, 30, 60)
        ).pack(side=tk.LEFT)

        ttk.Entry(
            bot_frame,
            textvariable=self.csv_path_var
        ).pack(
            side=tk.LEFT,
            fill=tk.X,
            expand=True,
            padx=(10, 4)
        )

        ttk.Button(
            bot_frame,
            text="...",
            width=3,
            command=self._choose_file
        ).pack(side=tk.LEFT)
        

    def _change_history(self):

        minutes = self.diagram_minutes_var.get()
        new_size = minutes * 60

        for key in self.data:
            self.data[key] = deque(
                self.data[key],
                maxlen=new_size
            )

        self.max_points = new_size
        self._redraw_charts()

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def _choose_file(self):
        path = filedialog.asksaveasfilename(
            title="CSV-Datei wählen",
            initialdir=os.path.dirname(self.csv_path_var.get()) or "C:\\",
            initialfile=os.path.basename(self.csv_path_var.get()) or "raumtemperatur.csv",
            defaultextension=".csv",
            filetypes=[("CSV-Dateien", "*.csv"), ("Alle Dateien", "*.*")]
        )
        if path:
            self.csv_path_var.set(path)

    def _toggle_connection(self):
        if self.running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.port_var.get()

        if not port:
            messagebox.showwarning("Hinweis", "Bitte zuerst einen COM-Port auswählen.")
            return

        try:
            self.ser = serial.Serial(port, baudrate=9600, timeout=0.5)

            self.running = True
            self.conn_btn.config(text="Disconnect")
            self.port_combo.config(state="disabled")
            self.status_var.set(f"Verbunden ({port})")

            self.rx_thread = threading.Thread(
                target=self._serial_worker,
                daemon=True
            )
            self.rx_thread.start()

        except Exception as e:
            messagebox.showerror(
                "Verbindungsfehler",
                f"Port {port} konnte nicht geöffnet werden:\n{e}"
            )

    def _disconnect(self):
        self.running = False

        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass

        self.ser = None
        self.conn_btn.config(text="Connect")
        self.port_combo.config(state="readonly")
        self.status_var.set("Getrennt")

    def _serial_worker(self):
        while self.running and self.ser:
            try:
                raw_line = self.ser.readline()
                if not raw_line:
                    continue
                line = raw_line.decode("utf-8", errors="ignore").strip()
                match = self.pattern.search(line)
                if match:
                    vals = {
                        "stts_t": float(match.group(1)),
                        "hts_t":  float(match.group(2)),
                        "hts_rh": float(match.group(3)),
                        "lps_t":  float(match.group(4)),
                        "lps_p":  float(match.group(5)),
                    }
                    self.root.after(0, self._process_new_data, vals)
            except Exception:
                if self.running:
                    self.root.after(0, self._disconnect)
                break

    def _process_new_data(self, vals: dict):
        for key in self.data:
            v = vals[key]
            self.data[key].append(v)

            title, lbl = self.val_labels[key]
            unit = title.split("(")[-1].rstrip(")")
            short_name = title.split(" ")[0]

            lbl.config(text=f"{short_name}: {v:.1f}{unit}")

        self.pressure_var.set(
            f"Druck: {vals['lps_p']:.1f} hPa"
        )

        if self.log_enabled_var.get():

            now = time.time()

            if now - self.last_log_time >= self.log_interval_var.get():

                self._append_csv(vals)
                self.last_log_time = now

        # IMMER zeichnen
        self._redraw_charts()

    def _append_csv(self, vals: dict):
        path = self.csv_path_var.get().strip()
        if not path:
            return
        try:
            folder = os.path.dirname(path)
            if folder:
                os.makedirs(folder, exist_ok=True)
            file_exists = os.path.isfile(path)
            with open(path, mode="a", newline="", encoding="utf-8") as f:
                writer = csv.writer(f, delimiter=";")
                if not file_exists:
                    writer.writerow([
                        "Zeitstempel",
                        "STTS_Temp_C",
                        "HTS_Temp_C",
                        "HTS_Feuchte_pct",
                        "LPS_Temp_C",
                        "LPS_Druck_hPa"
                    ])
                writer.writerow([
                    datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    f"{vals['stts_t']:.1f}",
                    f"{vals['hts_t']:.1f}",
                    f"{vals['hts_rh']:.1f}",
                    f"{vals['lps_t']:.1f}",
                    f"{vals['lps_p']:.1f}",
                ])
        except Exception as e:
            self.log_enabled_var.set(False)
            messagebox.showerror("CSV-Schreibfehler", f"Logging deaktiviert:\n{e}")

    def _draw_subplot(self, x0, y0, x1, y1, series_list, unit_suffix, grid_steps):
        #"""Zeichnet ein einzelnes Teil-Diagramm inkl. Achsenbeschriftung."""
        plot_w = x1 - x0
        plot_h = y1 - y0
        # Rahmen
        self.canvas.create_rectangle(
        x0, y0, x1, y1,
        outline="#dddddd"
        )

        # Rasterlinien
        for i in range(1, grid_steps):
            y = y0 + (plot_h * i / grid_steps)
            self.canvas.create_line(
                x0, y, x1, y,
                fill="#f0f0f0"
            )

        all_vals = []

        for key, _ in series_list:
            all_vals.extend(self.data[key])
            if len(all_vals) < 2:
                return

        ymin = min(all_vals) - 0.5
        ymax = max(all_vals) + 0.5

        if ymax - ymin < 1.0:
            ymax = ymin + 1.0

        self.canvas.create_text(
            x0 - 4,
            y0 + 5,
            text=f"{ymax:.1f}{unit_suffix}",
            anchor="e",
            fill="#555555",
            font=("Segoe UI", 7)
        )

        self.canvas.create_text(
            x0 - 4,
            y1 - 4,
            text=f"{ymin:.1f}{unit_suffix}",
            anchor="e",
            fill="#555555",
            font=("Segoe UI", 7)
        )

        x_step = plot_w / max(1, self.max_points - 1)

        for key, color in series_list:

            pts = list(self.data[key])

            if len(pts) < 2:
                continue

            coords = []

            for idx, val in enumerate(pts):
                x = x0 + idx * x_step

                norm = (val - ymin) / (ymax - ymin)
                y = y1 - (norm * plot_h)

                coords.extend([x, y])

            self.canvas.create_line(
                *coords,
                fill=color,
                width=2,
                smooth=False
            )
    
    def _redraw_charts(self):
        self.canvas.delete("all")
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        if w < 60 or h < 80:
            return
        if len(self.data["stts_t"]) < 2:
            self.canvas.create_text(w / 2, h / 2, text="Warte auf Sensordaten...", fill="#888888")
            return
        pad_l, pad_r, pad_t, pad_b, gap = 46, 12, 10, 10, 12
        avail_h = h - pad_t - pad_b - gap
        # Verhältnis 2 : 1 -> Feuchte-Diagramm ist genau halb so hoch wie Temperatur-Diagramm
        temp_h = avail_h * (2.0 / 3.0)
        rh_h = avail_h * (1.0 / 3.0)
        x0 = pad_l
        x1 = w - pad_r
        # Oberes Diagramm: Temperaturen (2/3 Höhe)
        t_y0 = pad_t
        t_y1 = t_y0 + temp_h
        temp_series = [(k, c) for k, _, c in self.temp_cfg]
        self._draw_subplot(x0, t_y0, x1, t_y1, temp_series, "°", grid_steps=4)
        # Unteres Diagramm: Luftfeuchte (1/3 Höhe = halb so hoch)
        rh_y0 = t_y1 + gap
        rh_y1 = rh_y0 + rh_h
        self._draw_subplot(x0, rh_y0, x1, rh_y1, [("hts_rh", self.rh_color)], "%", grid_steps=2)
        # Zeitbereich anzeigen
        self.canvas.create_text(
            x1,
            h - 2,
            text=f"{self.diagram_minutes_var.get()} min",
            anchor="se",
            fill="#555555",
            font=("Segoe UI", 8)
        )
    
    def _on_close(self):
        self._disconnect()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = STM32SensorMonitor(root)
    root.mainloop()