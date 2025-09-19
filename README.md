**LVGL StellarBridge** is a project that turns a compact embedded system into a **portable, intelligent network bridge**.  
It combines **network address translation (NAT)**, a **web-based control panel**, and a **futuristic graphical display** — creating a self-contained system that feels like a gateway from another world.  

---

## ✨ Features

- **Network Bridging with NAT**
  - Connects to an upstream Wi-Fi network (Station mode).
  - Creates its own Wi-Fi Access Point for clients.
  - Clients access the internet transparently through NAT.

- **Web Configuration Interface**
  - Configure SSID, password, and NAT settings.
  - Scan and select nearby networks.
  - Over-the-air (OTA) firmware update support.
  - Monitor connected clients.

- **Futuristic Display UI (LVGL)**
  - Real-time Wi-Fi signal strength indicators.
  - Battery monitoring & charging icons.
  - Animated sci-fi style hotspot and status logos.

- **System Management**
  - Hardware button input (e.g., power/display control).
  - Scheduled restart and keep-alive timers.
  - Persistent configuration via NVS.

---

## 📂 Project Structure

- `esp32_nat_router.c` – Core network logic, NAT setup, AP/STA configuration.  
- `http_server.c` – Lightweight web server for UI and API endpoints.  
- `scan.c` – Wi-Fi scan module, logs nearby APs and saves results.  
- `timer.c` – Restart scheduler and keep-alive timer with web ping.  
- `display.h` / `*.c` – LVGL display logic, icons, themes, charging animation.  
- `cmd_decl.h` – Command declarations for system/router controls.  
- `lv_conf.h` – LVGL configuration (graphics settings, memory, OS hooks).  
- `assets/*.c` – Sci-fi themed graphics: logos, arrows, radial backgrounds.  

---

## 🚀 Getting Started

### Prerequisites
- ESP-IDF v5.x (tested)
- LVGL v9.x
- Wi-Fi network for STA connection

### Build & Flash
```bash
idf.py set-target esp32
idf.py menuconfig   # (configure Wi-Fi, NAT options, LVGL if needed)
idf.py build
idf.py flash monitor
First Use
Power on device — it starts as an AP.

Connect to its Wi-Fi hotspot.

Visit the web UI (default: http://192.168.4.1).

Configure upstream Wi-Fi credentials.

Watch the cosmic display come alive ✨.

🌠 Screenshots / UI Preview
(Insert LVGL screenshots or photos of the device screen here)

🛠️ Future Work
Mesh networking support.

Sci-fi inspired custom themes for LVGL UI.

Data usage statistics on screen.

Additional OTA sources and cloud sync.
