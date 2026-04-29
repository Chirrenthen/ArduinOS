# ArduinOS

![ArduinOS](https://img.shields.io/badge/ArduinOS-v1.0-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.7+-3776AB?style=for-the-badge&logo=python&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-UNO_R3-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows_|_macOS_|_Linux-808080?style=for-the-badge)
![License](https://img.shields.io/badge/License-BSD-3lightgrey?style=for-the-badge)

> **Turn your Arduino UNO into an interactive shell — no recompiling needed.**

ArduinOS is a Linux-like operating environment for the Arduino UNO. A Python-powered terminal on your PC communicates with the Arduino over Serial, giving you a live shell to control GPIO, read sensors, manage a virtual filesystem, and even execute multi-command scripts — all without touching the Arduino ID

**Demo**
![Image](https://github.com/Chirrenthen/ArduinOS/blob/main/ArduinOS.png)

## 📦 Installation

### Requirements

- Arduino UNO R3 (ATmega328P)
- Python 3.7+
- USB cable

### Step 1 — Clone the repository

```bash
git clone https://github.com/chirrenthen/arduinOS.git
cd arduinOS
```

### Step 2 — Set up the Python environment

**macOS / Linux:**
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install pyserial readline
```

**Windows:**
```bat
python -m venv .venv
.venv\Scripts\activate.bat
pip install pyserial pyreadline3
```

### Step 3 — Flash the Arduino firmware

1. Open `arduino/arduino.ino` in the Arduino IDE
2. Select **Board:** `Arduino UNO`
3. Select the correct **Port**
4. Click **Upload**

### Step 4 — Launch the terminal
**macOS / Linux**
```bash
cd ArduinOS
source .venv/bin/activate
python terminal.py
```
**Windows**
```bash
cd ArduinOS
.venv\Scripts\activate.bat
python terminal.py
```

ArduinOS will automatically detect your Arduino's serial port and connect. If multiple ports are found, you'll be prompted to choose one.

---

## 🖥️ Terminal Features (Python side)

| Feature | Description |
|---|---|
| **Auto port detection** | Automatically finds and connects to your Arduino |
| **Tab completion** | Press `Tab` to complete any Arduino or terminal command |
| **Command history** | Use `↑` / `↓` arrows; history saved to `~/.arduinos_history` |
| **Macro engine** | Save and replay multi-command sequences |
| **Built-in editor** | Write scripts directly in the terminal with `!edit` |
| **File sync** | Sync your local workspace folder to the Arduino with `!sync` |
| **Animated UI** | Gradient banner, spinner, progress bar, matrix rain |
| **Background reader** | Serial output appears without interrupting your typing |

### Terminal-only commands (prefix with `!`)

```
!help               Show terminal help
!clear              Clear screen
!exit / !q          Exit ArduinOS
!ports              List available serial ports
!disconnect         Disconnect from Arduino
!reconnect          Reconnect to Arduino

!edit <file>        Open terminal editor to write a script
!upload <file>      Upload a local file to Arduino filesystem
!download <file>    Download a file from Arduino to your PC
!sync               Sync entire workspace folder to Arduino
!ls                 List files in local workspace

!macro              List all saved macros
!macro <name>       Run a saved macro
!macro <n> = <cmd>  Save a new macro
!anim               Matrix rain animation
```

---

## ⚡ Arduino Commands

All commands are sent live over Serial — no recompiling needed.

### Hardware Control

```bash
# GPIO
pinmode 13 output          # Set pin mode (INPUT, OUTPUT, INPUT_PULLUP, PWM)
write 13 HIGH              # Digital write (HIGH, LOW, ON, OFF, 1, 0)
gpio 13 on                 # Shorthand GPIO control
gpio 13 toggle             # Toggle pin state
read                       # Read all digital pins
read 7                     # Read specific pin

# Analog
aread                      # Read all analog pins (A0-A5) with voltage
aread A2                   # Read specific analog pin

# PWM
pwm 9 128                  # PWM output on pin 9 (0-255)

# Tone
tone 8 440 1000            # Play 440Hz on pin 8 for 1000ms
notone 8                   # Stop tone on pin 8
```

### Sensors & Monitoring

```bash
sensor                     # Read all analog inputs with bar graph
scope A0 100               # Mini oscilloscope — 100 samples from A0
monitor A0 500 10          # Live monitor A0 every 500ms for 10 seconds
```

### Scripting & Automation

```bash
# Run inline multi-command scripts
eval "gpio 13 on; delay 500; gpio 13 off"

# Loop a command
for 10 "gpio 13 toggle; delay 200"

# Write a script to the filesystem and run it
writefile blink.sh "gpio 13 on;delay 500;gpio 13 off;delay 500"
run blink.sh

# Delay
delay 1000                 # Wait 1000ms
```

### Virtual Filesystem

```bash
ls                         # List files in current directory
pwd                        # Print working directory
cd home                    # Change directory
cd ..                      # Go up one level
mkdir myproject            # Create directory
touch notes.txt            # Create empty file
writefile notes.txt hello  # Write content to file
cat notes.txt              # Read file content
rm notes.txt               # Delete file
```

> The filesystem lives in RAM — 8 files/directories max, 24 bytes per file. All data is lost on reboot. For persistence, use `!upload` / `!download` to sync with your PC.

### Fun & Extras

```bash
disco 5 30                 # LED light show (cycles, speed)
morse LED SOS              # Morse code output on any pin
wave                       # Draw wave ASCII art
neofetch                   # System info with ASCII art
```

### System Commands

```bash
help                       # Full command reference
sysinfo / neofetch         # System info (CPU, RAM, uptime, files)
uptime                     # Time since boot
dmesg                      # Kernel message log
free                       # Free RAM and file slots
reboot                     # Soft reset Arduino
whoami                     # Prints: root
uname                      # OS and platform info
clear                      # Clear screen and show logo
```

## 🔧 Macros

Macros let you save and replay sequences of commands from the Python terminal.

```bash
# Save a macro
!macro blink = eval "gpio 13 on; delay 300; gpio 13 off; delay 300"

# Run it anytime
!macro blink

# List all macros
!macro
```

Macros are saved to `~/.arduinos_macros.json` and persist across sessions.

---

## 📁 Workspace

ArduinOS creates a local workspace folder at `~/arduinos_workspace/`. You can write scripts there and sync them to the Arduino:

```bash
# In your terminal
!edit blink.sh       # Write a script
!upload blink.sh     # Upload to Arduino

# On the Arduino
run blink.sh         # Execute it
```

## 🛠️ Troubleshooting

**No ports found**
Make sure the USB cable supports data (not charge-only) and the Arduino driver is installed. On Linux you may need to add yourself to the `dialout` group: `sudo usermod -aG dialout $USER`

**Garbled output / no response**
Ensure the baud rate is set to `9600` in both `terminal.py` and the Serial Monitor if you test there.

**`readline` error on Windows**
Install `pyreadline3`: `pip install pyreadline3`

**Arduino not auto-detected**
Run `!ports` to see available ports and select manually when prompted.

---

## 🤝 Contributing

Pull requests are welcome! If you find a bug or have a feature idea, open an issue.

---

## Acknowledgements
I sincerely thank @PPPDUD and @Arc1011 — the tone and notone command implementations in this project are based on their work in KernelUNO.

<p align="center">Built with ❤️ for the Arduino community</p>
