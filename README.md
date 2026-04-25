# ArduinOS
![ArduinOS Banner](https://img.shields.io/badge/ArduinOS-v3.2-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.7+-3776AB?style=for-the-badge&logo=python&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-UNO_R3-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows_|_macOS_|_Linux-808080?style=for-the-badge)

> **Turn your Arduino UNO into a kernel with a beautiful Python-powered terminal. Write and execute code directly on the Arduino without compiling!**

## 📦 Installation

### Step 1: Clone the Repository

```bash
git clone https://github.com/chirrenthen/arduinOS.git
cd arduinOS
```
### Step 2: Setup environment and install libraries
```bash
python -m venv .venv
.venv\Scripts\activate.bat
pip install readline pyserial
```
### Step 3: Upload code and run
-> Upload `arduino.ino` into Arduino
```bash
cd arduinOS
.venv\Scripts\activate.bat
python terminal.py
```
