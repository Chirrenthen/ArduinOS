#!/usr/bin/env python3
"""
ArduinOS Terminal - serial link and local file manager.
"""

import os
import sys
import re
import json
import glob
import time
import threading
from datetime import datetime

import serial
import serial.tools.list_ports

from colours import Colors
from animation import Animation

# Platform detection
SYSTEM = sys.platform
IS_WINDOWS = SYSTEM.startswith('win')
IS_MAC = SYSTEM == 'darwin'
IS_LINUX = SYSTEM.startswith('linux')

if IS_WINDOWS:
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    import msvcrt

# Optional readline
try:
    import readline
except ImportError:
    readline = None


class ArduinOSTerminal:
    """
    Manages the serial connection to an Arduino running ArduinOS
    and handles local workspace synchronisation.
    """

    def __init__(self):
        self.ser = None
        self.port = None
        self.running = False
        self.history_file = os.path.expanduser("~/.arduinos_history")
        self.sync_dir = os.path.expanduser("~/arduinos_workspace")
        self.macros = {}

        os.makedirs(self.sync_dir, exist_ok=True)
        self._setup_readline()
        self._load_macros()

    # ------------------------------------------------------------------
    # Readline setup
    # ------------------------------------------------------------------
    def _setup_readline(self) -> None:
        """Configure tab completion and command history."""
        if readline is None:
            return

        try:
            readline.read_history_file(self.history_file)
        except FileNotFoundError:
            pass
        readline.set_history_length(1000)

        commands = [
            'help', 'ls', 'cd', 'pwd', 'mkdir', 'touch', 'cat', 'rm',
            'writefile', 'echo', 'gpio', 'read', 'aread', 'pwm',
            'pinmode', 'tone', 'notone', 'disco', 'sensor', 'scope',
            'morse', 'monitor', 'eval', 'exec', 'run', 'for', 'delay',
            'sysinfo', 'neofetch', 'uptime', 'dmesg', 'free', 'df',
            'whoami', 'uname', 'clear', 'reboot', 'wave',
            '!help', '!clear', '!sync', '!upload', '!download',
            '!edit', '!macro', '!anim', '!disconnect', '!reconnect'
        ]

        def completer(text, state):
            options = [c for c in commands if c.startswith(text)]
            return options[state] if state < len(options) else None

        readline.set_completer(completer)
        readline.parse_and_bind("tab: complete")

    # ------------------------------------------------------------------
    # Macros
    # ------------------------------------------------------------------
    def _load_macros(self) -> None:
        macro_file = os.path.expanduser("~/.arduinos_macros.json")
        try:
            with open(macro_file) as f:
                self.macros = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            self.macros = {
                "blink": 'eval "pinmode 13 output; for 5 \'gpio 13 toggle; delay 500\'"',
                "sweep": 'eval "for 10 \'gpio 2 on; delay 50; gpio 2 off; gpio 3 on; delay 50; gpio 3 off\'"',
                "hello": 'eval "gpio LED on; delay 300; gpio LED off; delay 300; gpio LED on; delay 300; gpio LED off"'
            }

    def _save_macros(self) -> None:
        macro_file = os.path.expanduser("~/.arduinos_macros.json")
        with open(macro_file, 'w') as f:
            json.dump(self.macros, f, indent=2)

    # ------------------------------------------------------------------
    # Display helpers
    # ------------------------------------------------------------------
    def clear_screen(self) -> None:
        os.system('cls' if IS_WINDOWS else 'clear')

    def show_banner(self) -> None:
        self.clear_screen()
        banner = f"""
    {Colors.BRIGHT_CYAN}                                  
░░      ░░░       ░░░       ░░░  ░░░░  ░░        ░░   ░░░  ░░░      ░░░░      ░░
▒  ▒▒▒▒  ▒▒  ▒▒▒▒  ▒▒  ▒▒▒▒  ▒▒  ▒▒▒▒  ▒▒▒▒▒  ▒▒▒▒▒    ▒▒  ▒▒  ▒▒▒▒  ▒▒  ▒▒▒▒▒▒▒
▓  ▓▓▓▓  ▓▓       ▓▓▓  ▓▓▓▓  ▓▓  ▓▓▓▓  ▓▓▓▓▓  ▓▓▓▓▓  ▓  ▓  ▓▓  ▓▓▓▓  ▓▓▓      ▓▓
█        ██  ███  ███  ████  ██  ████  █████  █████  ██    ██  ████  ████████  █
█  ████  ██  ████  ██       ████      ███        ██  ███   ███      ████      ██{Colors.RESET}

        {Colors.CYAN}System:{Colors.RESET} {SYSTEM}
        {Colors.CYAN}Python:{Colors.RESET} {sys.version.split()[0]}
        {Colors.CYAN}Workspace:{Colors.RESET} {self.sync_dir}
        
        {Colors.DIM}Type {Colors.YELLOW}'!help'{Colors.DIM} for terminal commands{Colors.RESET}
        {Colors.DIM}Type {Colors.YELLOW}'help'{Colors.DIM} for Arduino commands{Colors.RESET}
        {Colors.DIM}Press {Colors.YELLOW}Tab{Colors.DIM} for completion{Colors.RESET}
        {"─" * 55}
    """
        print(banner)

    def _show_local_help(self) -> None:
        help_text = f"""
         {Colors.CYAN}{Colors.BOLD}Terminal Commands{Colors.RESET}
         
         {Colors.GREEN}Local commands:{Colors.RESET}
           !help, !h       this help
           !clear, !cls    clear screen
           !exit, !q       quit terminal
           !ports          list serial ports
           !connect [port] connect to Arduino
           !disconnect     disconnect
           !reconnect      reconnect
         
         {Colors.GREEN}File operations:{Colors.RESET}
           !edit <file>    edit / create a file
           !upload <file>  upload file to Arduino
           !download <f>   download file from Arduino
           !sync           sync workspace -> Arduino
           !ls             list workspace files
         
         {Colors.GREEN}Automation:{Colors.RESET}
           !macro          list macros
           !macro <name>   run macro
           !macro <n> = <code>  save macro
         
         {Colors.GREEN}Effects:{Colors.RESET}
           !anim           matrix rain
         
         {Colors.GREEN}Arduino commands (sent directly):{Colors.RESET}
           Hardware: pinmode, write, read, aread, pwm, gpio, tone, disco
           Sensors:  sensor, scope, monitor, morse
           Files:    ls, cd, mkdir, touch, cat, rm, writefile
           Code:     eval "cmd1; cmd2", run <script>, for <n> "cmd"
           System:   help, sysinfo, neofetch, dmesg, uptime, free
         
         {Colors.DIM}Tab completes commands, Up/Down for history{Colors.RESET}
        """
        print(help_text)

    # ------------------------------------------------------------------
    # Serial port helpers
    # ------------------------------------------------------------------
    @staticmethod
    def list_ports():
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print(f"\n{Colors.RED}X No serial ports found!{Colors.RESET}")
            print(f"{Colors.YELLOW}  Is the Arduino connected?{Colors.RESET}\n")
            return None

        print(f"\n{Colors.CYAN}{Colors.BOLD}  Available Ports:{Colors.RESET}\n")
        for i, port in enumerate(ports):
            indicator = Colors.GREEN + "*" if "USB" in port.description or "Arduino" in port.description else Colors.YELLOW + "."
            print(f"  {indicator} [{Colors.BRIGHT_WHITE}{i}{Colors.RESET}] {Colors.GREEN}{port.device}{Colors.RESET}")
            print(f"     {Colors.DIM}{port.description}{Colors.RESET}")
            if port.hwid:
                print(f"     {Colors.DIM}HWID: {port.hwid[:50]}...{Colors.RESET}")
            print()
        return ports

    def connect(self, port: str = None) -> bool:
        if port is None:
            ports = self.list_ports()
            if not ports:
                return False

            # Prefer Arduino-like ports
            arduino_ports = [p for p in ports if "USB" in p.description or "Arduino" in p.description or "CH340" in p.description]
            if arduino_ports:
                port = arduino_ports[0].device
                print(f"{Colors.CYAN}Auto-selected: {port}{Colors.RESET}")
            else:
                try:
                    choice = input(f"{Colors.YELLOW}Select port [0]: {Colors.RESET}")
                    port = ports[0].device if choice == "" else ports[int(choice)].device
                except (ValueError, IndexError):
                    print(f"{Colors.RED}Invalid selection{Colors.RESET}")
                    return False

        try:
            Animation.spinner(2, f"Connecting to {port}")
            self.ser = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)          # Let Arduino reset
            self.ser.flushInput()
            self.port = port
            print(f"\n{Colors.GREEN}{Colors.BOLD}+ Connected!{Colors.RESET}\n")
            return True
        except serial.SerialException as e:
            print(f"\n{Colors.RED}X Connection failed: {e}{Colors.RESET}\n")
            return False

    def disconnect(self) -> None:
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"\n{Colors.YELLOW}Disconnected{Colors.RESET}")

    def send_command(self, cmd: str) -> None:
        """Send a text command over the serial line."""
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(f"{cmd}\r\n".encode())
                time.sleep(0.2)
            except serial.SerialException as e:
                print(f"{Colors.RED}Error: {e}{Colors.RESET}")
                self.disconnect()

    # ------------------------------------------------------------------
    # Serial reader thread
    # ------------------------------------------------------------------
    def read_serial(self) -> None:
        """Continuously read from the serial port and print."""
        buffer = ""
        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    buffer += data
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        line = line.strip('\r')
                        if line:
                            sys.stdout.write(f"{line}\n")
                            sys.stdout.flush()
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    print(f"\n{Colors.RED}Read error: {e}{Colors.RESET}")
                break

    # ------------------------------------------------------------------
    # File transfer helpers
    # ------------------------------------------------------------------
    def upload_file(self, filename: str, content: str) -> bool:
        """Upload a text file to the Arduino."""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return False
        content_escaped = content.replace('"', '\\"')
        commands = [
            f'touch {filename}',
            f'writefile {filename} "{content_escaped}"'
        ]
        for cmd in commands:
            self.ser.write(f"{cmd}\r\n".encode())
            time.sleep(0.3)
        return True

    def download_file(self, filename: str) -> str:
        """Download a text file from the Arduino."""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return None
        self.ser.write(f"cat {filename}\r\n".encode())
        time.sleep(0.5)
        content = ""
        timeout = time.time() + 2
        while time.time() < timeout:
            if self.ser.in_waiting:
                content += self.ser.read().decode('utf-8', errors='ignore')
                timeout = time.time() + 0.5   # reset timeout on new data
            time.sleep(0.05)
        return content.strip()

    # ------------------------------------------------------------------
    # Workspace synchronisation & editing
    # ------------------------------------------------------------------
    def sync_workspace(self) -> None:
        """Upload all local workspace files to the Arduino."""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return

        print(f"{Colors.CYAN}Syncing workspace...{Colors.RESET}")
        local_files = [f for f in os.listdir(self.sync_dir) if not f.startswith('.')]
        for filename in local_files:
            filepath = os.path.join(self.sync_dir, filename)
            if os.path.isfile(filepath):
                with open(filepath, 'r') as f:
                    content = f.read()
                Animation.spinner(0.5, f"Uploading {filename}")
                if self.upload_file(filename, content):
                    print(f"  {Colors.GREEN}+{Colors.RESET} {filename}")
        print(f"{Colors.GREEN}Sync complete!{Colors.RESET}\n")

    def edit_file(self, filename: str) -> None:
        """Open a mini editor for a workspace file."""
        filepath = os.path.join(self.sync_dir, filename)
        print(f"\n{Colors.CYAN}{Colors.BOLD}Editing: {filename}{Colors.RESET}")
        print(f"{Colors.DIM}Type your code. Use ; for multiple commands.{Colors.RESET}")
        print(f"{Colors.DIM}Press Ctrl+D (Unix) or Ctrl+Z (Windows) to finish.{Colors.RESET}\n")
        lines = []
        try:
            while True:
                line = input(f"{Colors.YELLOW}{len(lines)+1:03d}{Colors.RESET} | ")
                lines.append(line)
        except (EOFError, KeyboardInterrupt):
            pass
        content = '\n'.join(lines)
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"\n{Colors.GREEN}Saved {len(lines)} lines to {filename}{Colors.RESET}")
        if self.ser and self.ser.is_open:
            upload = input(f"{Colors.YELLOW}Upload to Arduino? [Y/n]: {Colors.RESET}").lower()
            if upload != 'n':
                self.upload_file(filename, content)

    # ------------------------------------------------------------------
    # Local command handler
    # ------------------------------------------------------------------
    def handle_local_command(self, cmd: str) -> bool:
        """
        Process commands prefixed with '!' or 'exit'.
        Returns True if the command was handled locally.
        """
        parts = cmd.split()
        if not parts:
            return False
        c = parts[0].lower()

        if c in ('!exit', '!quit', '!q', 'exit'):
            self.running = False
            return True

        if c in ('!help', '!h', '!?'):
            self._show_local_help()
            return True

        if c in ('!clear', '!cls'):
            self.clear_screen()
            self.show_banner()
            return True

        if c == '!ports':
            self.list_ports()
            return True

        if c == '!sync':
            self.sync_workspace()
            return True

        if c == '!upload':
            if len(parts) < 2:
                print(f"{Colors.YELLOW}Usage: !upload <filename>{Colors.RESET}")
            else:
                filepath = os.path.join(self.sync_dir, parts[1])
                if os.path.exists(filepath):
                    with open(filepath) as f:
                        self.upload_file(parts[1], f.read())
                else:
                    print(f"{Colors.RED}File not found in workspace{Colors.RESET}")
            return True

        if c == '!download':
            if len(parts) < 2:
                print(f"{Colors.YELLOW}Usage: !download <filename>{Colors.RESET}")
            else:
                content = self.download_file(parts[1])
                if content:
                    filepath = os.path.join(self.sync_dir, parts[1])
                    with open(filepath, 'w') as f:
                        f.write(content)
                    print(f"{Colors.GREEN}Downloaded: {parts[1]}{Colors.RESET}")
            return True

        if c == '!edit':
            if len(parts) < 2:
                print(f"{Colors.YELLOW}Usage: !edit <filename>{Colors.RESET}")
            else:
                self.edit_file(parts[1])
            return True

        if c == '!macro':
            if len(parts) < 2:
                print(f"\n{Colors.CYAN}Saved Macros:{Colors.RESET}")
                for name, code in self.macros.items():
                    print(f"  {Colors.GREEN}{name}{Colors.RESET}: {Colors.DIM}{code}{Colors.RESET}")
                print()
            elif len(parts) == 2:
                name = parts[1]
                if name in self.macros:
                    code = self.macros[name]
                    print(f"{Colors.CYAN}Running macro: {name}{Colors.RESET}")
                    print(f"  {Colors.DIM}{code}{Colors.RESET}")
                    self.send_command(code)
                else:
                    print(f"{Colors.RED}Macro '{name}' not found{Colors.RESET}")
            elif len(parts) >= 4 and parts[2] == '=':
                name = parts[1]
                code = ' '.join(parts[3:])
                self.macros[name] = code
                self._save_macros()
                print(f"{Colors.GREEN}Macro '{name}' saved{Colors.RESET}")
            return True

        if c == '!anim':
            print(f"\n{Colors.MAGENTA}")
            Animation.matrix_rain(5, 1.5)
            self.show_banner()
            return True

        if c == '!disconnect':
            self.disconnect()
            return True

        if c == '!reconnect':
            self.disconnect()
            time.sleep(1)
            self.connect(self.port)
            return True

        if c == '!ls':
            files = os.listdir(self.sync_dir)
            print(f"\n{Colors.CYAN}Workspace files:{Colors.RESET}")
            for f in sorted(files):
                print(f"  {Colors.GREEN}{f}{Colors.RESET}")
            print()
            return True

        return False

    # ------------------------------------------------------------------
    # Main run loop
    # ------------------------------------------------------------------
    def run(self) -> None:
        """Start the terminal interface."""
        self.show_banner()

        ports = list(serial.tools.list_ports.comports())
        if ports:
            if not self.connect():
                return
        else:
            print(f"{Colors.RED}No Arduino found. Connect and restart.{Colors.RESET}")
            print(f"{Colors.YELLOW}Type !help for options.{Colors.RESET}\n")

        self.running = True
        reader_thread = threading.Thread(target=self.read_serial, daemon=True)
        reader_thread.start()

        try:
            while self.running:
                prompt = f"{Colors.BRIGHT_GREEN}> {Colors.RESET}{Colors.DIM}arduinos{Colors.RESET} "
                try:
                    cmd = input(prompt).strip()
                except (EOFError, KeyboardInterrupt):
                    print()
                    break
                if not cmd:
                    continue
                if cmd.startswith('!') or cmd.startswith('exit'):
                    if self.handle_local_command(cmd):
                        continue
                else:
                    self.send_command(cmd)
        finally:
            # Save history if possible
            if readline:
                try:
                    readline.write_history_file(self.history_file)
                except Exception:
                    pass
            self.disconnect()
            print(f"\n{Colors.CYAN}{Colors.BOLD}")
            print("    Thanks for using ArduinOS!")
            print(f"    Your files are in: {self.sync_dir}")
            print(f"{Colors.RESET}\n")


def main():
    """Entry point for the terminal application."""
    terminal = ArduinOSTerminal()
    terminal.run()


if __name__ == "__main__":
    main()