#!/usr/bin/env python3
"""
ArduinOS Terminal v1.0
Elegant terminal with code editing, file sync, and visual effects
"""

import serial
import serial.tools.list_ports
import time
import os
import sys
import threading
import json
import re
import readline
import glob
from datetime import datetime

# === Platform Detection ===
SYSTEM = sys.platform
IS_WINDOWS = SYSTEM.startswith('win')
IS_MAC = SYSTEM == 'darwin'
IS_LINUX = SYSTEM.startswith('linux')

# Windows ANSI support
if IS_WINDOWS:
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    import msvcrt

class Colors:
    """Advanced color and style system"""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    ITALIC = '\033[3m'
    UNDERLINE = '\033[4m'
    BLINK = '\033[5m'
    REVERSE = '\033[7m'
    HIDDEN = '\033[8m'
    STRIKE = '\033[9m'
    
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    
    BRIGHT_BLACK = '\033[90m'
    BRIGHT_RED = '\033[91m'
    BRIGHT_GREEN = '\033[92m'
    BRIGHT_YELLOW = '\033[93m'
    BRIGHT_BLUE = '\033[94m'
    BRIGHT_MAGENTA = '\033[95m'
    BRIGHT_CYAN = '\033[96m'
    BRIGHT_WHITE = '\033[97m'
    
    @staticmethod
    def rgb(r, g, b, bg=False):
        """True color RGB support"""
        if bg:
            return f'\033[48;2;{r};{g};{b}m'
        return f'\033[38;2;{r};{g};{b}m'
    
    @staticmethod
    def gradient(text, start_rgb, end_rgb):
        """Color gradient text"""
        result = ""
        for i, char in enumerate(text):
            t = i / max(len(text) - 1, 1)
            r = int(start_rgb[0] + (end_rgb[0] - start_rgb[0]) * t)
            g = int(start_rgb[1] + (end_rgb[1] - start_rgb[1]) * t)
            b = int(start_rgb[2] + (end_rgb[2] - start_rgb[2]) * t)
            result += Colors.rgb(r, g, b) + char
        result += Colors.RESET
        return result

class Animation:
    """Terminal animation system"""
    
    @staticmethod
    def spinner(seconds, message="Processing"):
        """Show spinner animation"""
        chars = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
        for _ in range(seconds * 10):
            for char in chars:
                sys.stdout.write(f'\r{Colors.CYAN}{char} {message}...{Colors.RESET}')
                sys.stdout.flush()
                time.sleep(0.1)
        sys.stdout.write('\r' + ' ' * (len(message) + 10) + '\r')
        sys.stdout.flush()
    
    @staticmethod
    def progress_bar(total, prefix="Progress", length=30):
        """Animated progress bar"""
        for i in range(total + 1):
            percent = (i * 100) // total
            filled = int(length * i // total)
            bar = '█' * filled + '░' * (length - filled)
            sys.stdout.write(f'\r{prefix}: |{Colors.GREEN}{bar}{Colors.RESET}| {percent}%')
            sys.stdout.flush()
            time.sleep(0.05)
        print()
    
    @staticmethod
    def typewriter(text, speed=0.03):
        """Typewriter animation"""
        for char in text:
            sys.stdout.write(char)
            sys.stdout.flush()
            time.sleep(speed)
    
    @staticmethod
    def matrix_rain(lines=10, duration=2):
        """Matrix-style rain effect"""
        import random
        chars = "ﾊﾐﾋｰｳｼﾅﾓﾆｻﾜﾂｵﾘｱﾎﾃﾏｹﾒｴｶｷﾑﾕﾗｾﾈｽﾀﾇﾍ012345789:・.\"=*+-<>¦|╌"
        end_time = time.time() + duration
        
        while time.time() < end_time:
            line = ''.join(random.choice(chars) for _ in range(80))
            color = random.choice([Colors.GREEN, Colors.BRIGHT_GREEN, Colors.rgb(0, random.randint(100, 255), 0)])
            sys.stdout.write(f'{color}{line}{Colors.RESET}\n')
            sys.stdout.flush()
            time.sleep(0.05)
        
        os.system('cls' if IS_WINDOWS else 'clear')

class ArduinOSTerminal:
    """Advanced terminal application"""
    
    def __init__(self):
        self.ser = None
        self.port = None
        self.running = False
        self.history_file = os.path.expanduser("~/.arduinos_history")
        self.sync_dir = os.path.expanduser("~/arduinos_workspace")
        self.macros = {}
        
        # Create workspace
        os.makedirs(self.sync_dir, exist_ok=True)
        
        # Setup readline
        self._setup_readline()
        
        # Load macros
        self._load_macros()
    
    def _setup_readline(self):
        """Configure readline for history and completion"""
        try:
            import readline
        except ImportError:
            return
        
        # History
        try:
            readline.read_history_file(self.history_file)
        except FileNotFoundError:
            pass
        
        readline.set_history_length(1000)
        
        # Tab completion
        commands = ['help', 'ls', 'cd', 'pwd', 'mkdir', 'touch', 'cat', 'rm',
                    'writefile', 'echo', 'gpio', 'read', 'aread', 'pwm',
                    'pinmode', 'tone', 'notone', 'disco', 'sensor', 'scope',
                    'morse', 'monitor', 'eval', 'exec', 'run', 'for', 'delay',
                    'sysinfo', 'neofetch', 'uptime', 'dmesg', 'free', 'df',
                    'whoami', 'uname', 'clear', 'reboot', 'wave',
                    '!help', '!clear', '!sync', '!upload', '!download',
                    '!edit', '!macro', '!anim', '!disconnect', '!reconnect']
        
        def completer(text, state):
            options = [c for c in commands if c.startswith(text)]
            return options[state] if state < len(options) else None
        
        readline.set_completer(completer)
        readline.parse_and_bind("tab: complete")
    
    def _load_macros(self):
        """Load saved macros"""
        macro_file = os.path.expanduser("~/.arduinos_macros.json")
        try:
            with open(macro_file) as f:
                self.macros = json.load(f)
        except:
            self.macros = {
                "blink": "eval \"pinmode 13 output; for 5 'gpio 13 toggle; delay 500'\"",
                "sweep": "eval \"for 10 'gpio 2 on; delay 50; gpio 2 off; gpio 3 on; delay 50; gpio 3 off'\"",
                "hello": "eval \"gpio LED on; delay 300; gpio LED off; delay 300; gpio LED on; delay 300; gpio LED off\""
            }
    
    def _save_macros(self):
        """Save macros to file"""
        with open(os.path.expanduser("~/.arduinos_macros.json"), 'w') as f:
            json.dump(self.macros, f, indent=2)
    
    def clear_screen(self):
        """Clear terminal"""
        os.system('cls' if IS_WINDOWS else 'clear')
    
    def show_banner(self):
        """Display animated banner"""
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
    
    def list_ports(self):
        """List serial ports with details"""
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print(f"\n{Colors.RED}✗ No serial ports found!{Colors.RESET}")
            print(f"{Colors.YELLOW}  Is the Arduino connected?{Colors.RESET}\n")
            return None
        
        print(f"\n{Colors.CYAN}{Colors.BOLD}  Available Ports:{Colors.RESET}\n")
        for i, port in enumerate(ports):
            indicator = Colors.GREEN + "●" if "USB" in port.description or "Arduino" in port.description else Colors.YELLOW + "○"
            print(f"  {indicator} [{Colors.BRIGHT_WHITE}{i}{Colors.RESET}] {Colors.GREEN}{port.device}{Colors.RESET}")
            print(f"     {Colors.DIM}{port.description}{Colors.RESET}")
            if port.hwid:
                print(f"     {Colors.DIM}HWID: {port.hwid[:50]}...{Colors.RESET}")
            print()
        
        return ports
    
    def connect(self, port=None):
        """Connect to Arduino with animation"""
        if port is None:
            ports = self.list_ports()
            if not ports:
                return False
            
            # Auto-select Arduino port
            arduino_ports = [p for p in ports if "USB" in p.description or "Arduino" in p.description or "CH340" in p.description]
            if arduino_ports:
                port = arduino_ports[0].device
                print(f"{Colors.CYAN}Auto-selected: {port}{Colors.RESET}")
            else:
                try:
                    choice = input(f"{Colors.YELLOW}Select port [0]: {Colors.RESET}")
                    port = ports[0].device if choice == "" else ports[int(choice)].device
                except:
                    print(f"{Colors.RED}Invalid selection{Colors.RESET}")
                    return False
        
        try:
            Animation.spinner(2, f"Connecting to {port}")
            
            self.ser = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)  # Wait for Arduino reset
            self.ser.flushInput()
            self.port = port
            
            print(f"\n{Colors.GREEN}{Colors.BOLD}✓ Connected!{Colors.RESET}\n")
            return True
            
        except serial.SerialException as e:
            print(f"\n{Colors.RED}✗ Connection failed: {e}{Colors.RESET}\n")
            return False
    
    def disconnect(self):
        """Disconnect gracefully"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"\n{Colors.YELLOW}Disconnected{Colors.RESET}")
    
    def read_serial(self):
        """Background serial reader with buffering"""
        buffer = ""
        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    buffer += data
                    
                    # Print complete lines
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
    
    def upload_file(self, filename, content):
        """Upload file to Arduino"""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return False
        
        # Escape content
        content_escaped = content.replace('"', '\\"').replace('\n', '\\n')
        
        commands = [
            f'touch {filename}',
            f'writefile {filename} "{content_escaped}"'
        ]
        
        for cmd in commands:
            self.ser.write(f"{cmd}\r\n".encode())
            time.sleep(0.3)
        
        return True
    
    def download_file(self, filename):
        """Download file from Arduino"""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return None
        
        # Request file content
        self.ser.write(f"cat {filename}\r\n".encode())
        time.sleep(0.5)
        
        # Read response
        content = ""
        timeout = time.time() + 2
        while time.time() < timeout:
            if self.ser.in_waiting:
                content += self.ser.read().decode('utf-8', errors='ignore')
                timeout = time.time() + 0.5
            time.sleep(0.05)
        
        return content.strip()
    
    def sync_workspace(self):
        """Sync local files to Arduino"""
        if not self.ser or not self.ser.is_open:
            print(f"{Colors.RED}Not connected{Colors.RESET}")
            return
        
        print(f"{Colors.CYAN}Syncing workspace...{Colors.RESET}")
        
        # List local files
        local_files = [f for f in os.listdir(self.sync_dir) if not f.startswith('.')]
        
        for filename in local_files:
            filepath = os.path.join(self.sync_dir, filename)
            if os.path.isfile(filepath):
                with open(filepath, 'r') as f:
                    content = f.read()
                
                Animation.spinner(0.5, f"Uploading {filename}")
                if self.upload_file(filename, content):
                    print(f"  {Colors.GREEN}✓{Colors.RESET} {filename}")
        
        print(f"{Colors.GREEN}Sync complete!{Colors.RESET}\n")
    
    def edit_file(self, filename):
        """Edit file in terminal"""
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
        
        # Ask to upload
        if self.ser and self.ser.is_open:
            upload = input(f"{Colors.YELLOW}Upload to Arduino? [Y/n]: {Colors.RESET}").lower()
            if upload != 'n':
                self.upload_file(filename, content)
    
    def handle_local_command(self, cmd):
        """Handle terminal-specific commands"""
        parts = cmd.split()
        if not parts:
            return False
        
        c = parts[0].lower()
        
        if c in ['!exit', '!quit', '!q', 'exit']:
            self.running = False
            return True
        
        elif c in ['!help', '!h', '!?']:
            self._show_local_help()
            return True
        
        elif c in ['!clear', '!cls']:
            self.clear_screen()
            self.show_banner()
            return True
        
        elif c == '!ports':
            self.list_ports()
            return True
        
        elif c == '!sync':
            self.sync_workspace()
            return True
        
        elif c == '!upload':
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
        
        elif c == '!download':
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
        
        elif c == '!edit':
            if len(parts) < 2:
                print(f"{Colors.YELLOW}Usage: !edit <filename>{Colors.RESET}")
            else:
                self.edit_file(parts[1])
            return True
        
        elif c == '!macro':
            if len(parts) < 2:
                # List macros
                print(f"\n{Colors.CYAN}Saved Macros:{Colors.RESET}")
                for name, code in self.macros.items():
                    print(f"  {Colors.GREEN}{name}{Colors.RESET}: {Colors.DIM}{code}{Colors.RESET}")
                print()
            elif len(parts) == 2:
                # Run macro
                name = parts[1]
                if name in self.macros:
                    code = self.macros[name]
                    print(f"{Colors.CYAN}Running macro: {name}{Colors.RESET}")
                    print(f"  {Colors.DIM}{code}{Colors.RESET}")
                    self.send_command(code)
                else:
                    print(f"{Colors.RED}Macro '{name}' not found{Colors.RESET}")
            elif len(parts) >= 4 and parts[2] == '=':
                # Save macro
                name = parts[1]
                code = ' '.join(parts[3:])
                self.macros[name] = code
                self._save_macros()
                print(f"{Colors.GREEN}Macro '{name}' saved{Colors.RESET}")
            return True
        
        elif c == '!anim':
            print(f"\n{Colors.MAGENTA}")
            Animation.matrix_rain(5, 1.5)
            self.show_banner()
            return True
        
        elif c == '!disconnect':
            self.disconnect()
            return True
        
        elif c == '!reconnect':
            self.disconnect()
            time.sleep(1)
            self.connect(self.port)
            return True
        
        elif c == '!ls':
            files = os.listdir(self.sync_dir)
            print(f"\n{Colors.CYAN}Workspace files:{Colors.RESET}")
            for f in sorted(files):
                print(f"  {Colors.GREEN}{f}{Colors.RESET}")
            print()
            return True
        
        return False
    
    def _show_local_help(self):
        """Show local terminal help"""
        help_text = f"""
{Colors.CYAN}{Colors.BOLD}╔══════════════════════════════════════════════╗
                          ║              Terminal Commands               ║
                          ╚══════════════════════════════════════════════╝{Colors.RESET}

{Colors.GREEN}Local Commands:{Colors.RESET}
  !help, !h       Show this help
  !clear, !cls    Clear screen
  !exit, !q       Exit terminal
  !ports          List serial ports
  !connect [port] Connect to Arduino
  !disconnect     Disconnect
  !reconnect      Reconnect

{Colors.GREEN}File Operations:{Colors.RESET}
  !edit <file>    Edit/create file in terminal
  !upload <file>  Upload file to Arduino
  !download <f>   Download file from Arduino
  !sync           Sync workspace to Arduino
  !ls             List workspace files

{Colors.GREEN}Automation:{Colors.RESET}
  !macro          List macros
  !macro <name>   Run macro
  !macro <n> = <code>  Save macro

{Colors.GREEN}Effects:{Colors.RESET}
  !anim           Matrix rain animation

{Colors.GREEN}Arduino Commands (sent directly):{Colors.RESET}
  Hardware:  pinmode, write, read, aread, pwm, gpio, tone, disco
  Sensors:   sensor, scope, monitor, morse
  Files:     ls, cd, mkdir, touch, cat, rm, writefile
  Code:      eval "cmd1; cmd2", run <script>, for <n> "cmd"
  System:    help, sysinfo, neofetch, dmesg, uptime, free

{Colors.DIM}Tip: Use Tab for command completion
Tip: Use Up/Down arrows for history
Tip: Type code directly with eval: eval "gpio 13 on; delay 500; gpio 13 off"{Colors.RESET}
"""
        print(help_text)
    
    def send_command(self, cmd):
        """Send command to Arduino"""
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(f"{cmd}\r\n".encode())
                time.sleep(0.2)
            except serial.SerialException as e:
                print(f"{Colors.RED}Error: {e}{Colors.RESET}")
                self.disconnect()
    
    def run(self):
        """Main application loop"""
        self.show_banner()
        
        # Auto-connect
        ports = list(serial.tools.list_ports.comports())
        if ports:
            if not self.connect():
                return
        else:
            print(f"{Colors.RED}No Arduino found. Connect and restart.{Colors.RESET}")
            print(f"{Colors.YELLOW}Type !help for options.{Colors.RESET}\n")
        
        # Start reader thread
        self.running = True
        reader = threading.Thread(target=self.read_serial, daemon=True)
        reader.start()
        
        # Main input loop
        try:
            while self.running:
                prompt = f"{Colors.BRIGHT_GREEN}➜{Colors.RESET} {Colors.DIM}arduinos{Colors.RESET} "
                
                try:
                    cmd = input(prompt).strip()
                except (EOFError, KeyboardInterrupt):
                    print()
                    break
                
                if not cmd:
                    continue
                
                # Process local commands
                if cmd.startswith('!') or cmd.startswith('exit'):
                    if self.handle_local_command(cmd):
                        continue
                else:
                    self.send_command(cmd)
        
        finally:
            # Save history
            try:
                import readline
                readline.write_history_file(self.history_file)
            except:
                pass
            
            # Cleanup
            self.disconnect()
            print(f"\n{Colors.CYAN}{Colors.BOLD}")
            print("    Thanks for using ArduinOS!")
            print(f"    Your files are in: {self.sync_dir}")
            print(f"{Colors.RESET}\n")

def main():
    terminal = ArduinOSTerminal()
    terminal.run()

if __name__ == "__main__":
    main()
