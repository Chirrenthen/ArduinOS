/*
 * ArduinOS v3.0 - Advanced Kernel
 * Full GPIO, Analog, PWM control
 * Dynamic filesystem with command-line coding
 * Optimized for Arduino UNO (ATmega328P)
 */

#include <Arduino.h>
#include <string.h>
#include <avr/pgmspace.h>

// === Configuration ===
//#define MAX_FILES 16
//#define NAME_LEN 13
//#define CONTENT_LEN 48
//#define PATH_LEN 24
//#define DMESG_LINES 6
//#define DMESG_LEN 40
//#define CMD_LEN 40
//#define MAX_ARGS 4

#define MAX_FILES 8
#define CONTENT_LEN 24
#define PATH_LEN 16
#define DMESG_LINES 3
#define DMESG_LEN 24
#define CMD_LEN 32
#define NAME_LEN 10
#define MAX_ARGS 4

// === File Structure ===
typedef struct {
  char name[NAME_LEN];
  char content[CONTENT_LEN];
  char parentDir[PATH_LEN];
  uint8_t flags; // bit0=active, bit1=isDir, bit2=system
  unsigned long created;
} RAMFile;

#define IS_ACTIVE(f) (f.flags & 1)
#define IS_DIR(f) (f.flags & 2)
#define IS_SYSTEM(f) (f.flags & 4)

// === Kernel Log Structure ===
typedef struct {
  unsigned long timestamp;
  char message[DMESG_LEN];
} DmesgEntry;

// === Global State ===
RAMFile fs[MAX_FILES];
char currentPath[PATH_LEN] = "/";
char inputBuffer[CMD_LEN];
int8_t inputLen = 0;
DmesgEntry dmesg[DMESG_LINES];
uint8_t dmesgIndex = 0;
uint8_t dmesgCount = 0;
unsigned long bootTime;
char* args[MAX_ARGS];
uint8_t argCount = 0;

// === PROGMEM String Tables ===
const char S_EMPTY[] PROGMEM = "(empty)";
const char S_OK[] PROGMEM = "OK";
const char S_FAIL[] PROGMEM = "FAIL";
const char S_NOT_FOUND[] PROGMEM = "Not found";
const char S_NOSPACE[] PROGMEM = "No space left";
const char S_ROOT[] PROGMEM = "root";
const char S_PROMPT[] PROGMEM = "root@arduinos";
const char S_UNKNOWN[] PROGMEM = "Command not found. Type 'help'";

// PROGMEM print helper
void pprint(const char* pgm_str) {
  char c;
  while ((c = pgm_read_byte(pgm_str++))) Serial.print(c);
}

void pprintln(const char* pgm_str) {
  pprint(pgm_str);
  Serial.println();
}

// === Kernel Logging ===
void klog(const char* msg) {
  uint8_t idx = dmesgIndex % DMESG_LINES;
  dmesg[idx].timestamp = (millis() - bootTime) / 1000;
  strncpy(dmesg[idx].message, msg, DMESG_LEN - 1);
  dmesg[idx].message[DMESG_LEN - 1] = 0;
  dmesgIndex++;
  if (dmesgCount < DMESG_LINES) dmesgCount++;
}

// === Safe Reset ===
void(* softReset)(void) = 0;

// === Free RAM Check ===
int freeRAM() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// === String Utilities ===
int8_t strpos(const char* haystack, const char* needle) {
  int8_t hlen = strlen(haystack);
  int8_t nlen = strlen(needle);
  if (nlen == 0 || nlen > hlen) return -1;
  for (int8_t i = 0; i <= hlen - nlen; i++) {
    if (strncmp(haystack + i, needle, nlen) == 0) return i;
  }
  return -1;
}

int safeAtoi(const char* str) {
  int num = 0;
  int sign = 1;
  if (*str == '-') { sign = -1; str++; }
  while (*str >= '0' && *str <= '9') {
    num = num * 10 + (*str - '0');
    str++;
  }
  return num * sign;
}

float safeAtof(const char* str) {
  float num = 0.0;
  float dec = 0.0;
  float div = 10.0;
  int sign = 1;
  
  if (*str == '-') { sign = -1; str++; }
  
  while (*str >= '0' && *str <= '9') {
    num = num * 10.0 + (*str - '0');
    str++;
  }
  
  if (*str == '.') {
    str++;
    while (*str >= '0' && *str <= '9') {
      dec += (*str - '0') / div;
      div *= 10.0;
      str++;
    }
  }
  
  return sign * (num + dec);
}

void strlower(char* str) {
  for (uint8_t i = 0; str[i]; i++) {
    if (str[i] >= 'A' && str[i] <= 'Z') str[i] += 32;
  }
}

// === Command Parser ===
void parseCommand(char* line, char** argv, uint8_t* argc) {
  *argc = 0;
  char* token = strtok(line, " ");
  while (token != NULL && *argc < MAX_ARGS) {
    argv[*argc] = token;
    (*argc)++;
    token = strtok(NULL, " ");
  }
}

// === Filesystem Operations ===
int8_t findFile(const char* name, const char* path) {
  for (uint8_t i = 0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && 
        strcmp(fs[i].name, name) == 0 && 
        strcmp(fs[i].parentDir, path) == 0) {
      return i;
    }
  }
  return -1;
}

int8_t findFreeSlot() {
  for (uint8_t i = 0; i < MAX_FILES; i++) {
    if (!IS_ACTIVE(fs[i])) return i;
  }
  return -1;
}

void initFileSystem() {
  memset(fs, 0, sizeof(fs));
  
  // Create root directories
  const char* dirs[] = {"home", "dev", "tmp", "etc"};
  for (uint8_t d = 0; d < 4; d++) {
    int8_t slot = findFreeSlot();
    if (slot >= 0) {
      strncpy(fs[slot].name, dirs[d], NAME_LEN - 1);
      strncpy(fs[slot].parentDir, "/", PATH_LEN - 1);
      fs[slot].flags = 3; // active + directory
      fs[slot].created = millis();
    }
  }
  
  klog("Filesystem initialized");
}

// === Pin Name Resolution ===
int8_t resolvePin(const char* name) {
  // Digital pins
  if (name[0] == 'D' || name[0] == 'd') {
    int p = safeAtoi(name + 1);
    if (p >= 2 && p <= 13) return p;
  }
  
  // Direct number
  if (name[0] >= '0' && name[0] <= '9') {
    int p = safeAtoi(name);
    if (p >= 2 && p <= 13) return p;
  }
  
  // Analog pins
  if ((name[0] == 'A' || name[0] == 'a') && name[1] >= '0' && name[1] <= '5') {
    return 14 + (name[1] - '0'); // A0-A5 map to 14-19
  }
  
  // Special names
  if (strcmp(name, "LED") == 0 || strcmp(name, "led") == 0) return LED_BUILTIN;
  if (strcmp(name, "TX") == 0 || strcmp(name, "tx") == 0) return 1;
  if (strcmp(name, "RX") == 0 || strcmp(name, "rx") == 0) return 0;
  
  return -1;
}

// === ASCII Art Generator ===
//void showLogo() {
//  Serial.println(F("\n\
//  \x1B[36m   ╔══════════════════════════════╗\x1B[0m\n\
//  \x1B[36m   ║   \x1B[32mArduinOS v3.0\x1B[36m               ║\x1B[0m\n\
//  \x1B[36m   ║   \x1B[33mKernelUNO Production\x1B[36m        ║\x1B[0m\n\
//  \x1B[36m   ╚══════════════════════════════╝\x1B[0m\n\
//  \x1B[90m   ┌────────────────────────────┐\x1B[0m\n\
//  \x1B[90m   │\x1B[0m  \x1B[37mCPU:\x1B[0m ATmega328P @ 16MHz  \x1B[90m│\x1B[0m\n\
//  \x1B[90m   │\x1B[0m  \x1B[37mRAM:\x1B[0m %4d bytes free     \x1B[90m│\x1B[0m\n\
//  \x1B[90m   │\x1B[0m  \x1B[37mPins:\x1B[0m 14 Digital, 6 ADC \x1B[90m│\x1B[0m\n\
//  \x1B[90m   └────────────────────────────┘\x1B[0m\n", freeRAM());
//  Serial.println(F("  Type \x1B[33m'help'\x1B[0m for commands\n"));
//}

void showLogo() {
  Serial.println(F("\n\
  \x1B[36m   ╔══════════════════════════════╗\x1B[0m\n\
  \x1B[36m   ║ \x1B[32mArduinOS v3.0\x1B[36m║\x1B[0m\n\
  \x1B[36m   ║\x1B[33mKernelUNO Production\x1B[36m║\x1B[0m\n\
  \x1B[36m   ╚══════════════════════════════╝\x1B[0m\n\
  \x1B[90m   ┌────────────────────────────┐\x1B[0m"));

  Serial.println(F("  \x1B[90m   │\x1B[0m  \x1B[37mCPU:\x1B[0m ATmega328P @ 16MHz  \x1B[90m│\x1B[0m"));

  Serial.print(F("  \x1B[90m   │\x1B[0m  \x1B[37mRAM:\x1B[0m "));
  Serial.print(freeRAM());
  Serial.println(F(" bytes free     \x1B[90m│\x1B[0m"));

  Serial.println(F("  \x1B[90m   │\x1B[0m  \x1B[37mPins:\x1B[0m 14 Digital, 6 ADC \x1B[90m│\x1B[0m"));
  Serial.println(F("  \x1B[90m   └────────────────────────────┘\x1B[0m"));
  Serial.println(F("  Type \x1B[33m'help'\x1B[0m for commands\n"));
}

void drawWave() {
  Serial.println(F("\n\
  \x1B[36m     ╭╮              ╭╮\x1B[0m\n\
  \x1B[36m    ╭╯╰╮            ╭╯╰╮\x1B[0m\n\
  \x1B[36m   ╭╯  ╰╮          ╭╯  ╰╮\x1B[0m\n\
  \x1B[36m  ╭╯    ╰╮        ╭╯    ╰╮\x1B[0m\n\
  \x1B[36m ╭╯      ╰╮      ╭╯      ╰╮\x1B[0m\n\
  \x1B[36m╭╯        ╰╮────╭╯        ╰╮\x1B[0m\n"));
}

// === Hardware Commands ===
void cmdPinMode(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: pinmode <pin> <mode>"));
    Serial.println(F("Modes: INPUT, OUTPUT, INPUT_PULLUP, PWM"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin. Use: D2-D13, A0-A5, LED, 2-13"));
    return;
  }
  
  strlower(argv[2]);
  
  if (strcmp(argv[2], "output") == 0 || strcmp(argv[2], "out") == 0) {
    pinMode(pin, OUTPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> OUTPUT"));
  } else if (strcmp(argv[2], "input_pullup") == 0 || strcmp(argv[2], "pullup") == 0) {
    pinMode(pin, INPUT_PULLUP);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT_PULLUP"));
  } else if (strcmp(argv[2], "pwm") == 0) {
    if (pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11) {
      pinMode(pin, OUTPUT);
      Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> PWM ready"));
    } else {
      Serial.println(F("PWM only on pins 3,5,6,9,10,11"));
    }
  } else {
    pinMode(pin, INPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT"));
  }
  
  char buf[32];
  snprintf(buf, sizeof(buf), "pinMode %d %s", pin, argv[2]);
  klog(buf);
}

void cmdDigitalWrite(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: write <pin> <HIGH|LOW|1|0|ON|OFF>"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  strlower(argv[2]);
  int value = (strcmp(argv[2], "high") == 0 || 
               strcmp(argv[2], "1") == 0 || 
               strcmp(argv[2], "on") == 0) ? HIGH : LOW;
  
  pinMode(pin, OUTPUT);
  digitalWrite(pin, value);
  Serial.print(F("Pin ")); Serial.print(pin);
  Serial.print(F(" = ")); Serial.println(value ? F("HIGH") : F("LOW"));
  
  char buf[32];
  snprintf(buf, sizeof(buf), "Write pin %d = %d", pin, value);
  klog(buf);
}

void cmdDigitalRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: read <pin>"));
    // Read all pins if no argument
    Serial.println(F("\n  \x1B[33m=== Digital Pin States ===\x1B[0m"));
    Serial.println(F("  Pin | State | Mode"));
    Serial.println(F("  ----|-------|------"));
    for (int p = 2; p <= 13; p++) {
      pinMode(p, INPUT_PULLUP);
      delay(2);
      Serial.print(F("  "));
      if (p < 10) Serial.print(' ');
      Serial.print(p);
      Serial.print(F("  |  "));
      int v = digitalRead(p);
      Serial.print(v ? F("HIGH ") : F("LOW  "));
      Serial.print(F(" | INPUT_PULLUP"));
      Serial.println();
    }
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  pinMode(pin, INPUT_PULLUP);
  delay(5);
  int value = digitalRead(pin);
  Serial.print(F("Pin ")); Serial.print(pin);
  Serial.print(F(" = ")); Serial.println(value ? F("HIGH (1)") : F("LOW (0)"));
}

void cmdAnalogRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    // Read all analog pins
    Serial.println(F("\n  \x1B[33m=== Analog Inputs ===\x1B[0m"));
    Serial.println(F("  Pin  | Raw  | Voltage"));
    Serial.println(F("  -----|------|--------"));
    for (int a = 0; a <= 5; a++) {
      int raw = analogRead(a);
      float voltage = raw * (5.0 / 1023.0);
      Serial.print(F("  A"));
      Serial.print(a);
      Serial.print(F("   | "));
      Serial.print(raw);
      Serial.print(F("  | "));
      Serial.print(voltage, 2);
      Serial.println(F("V"));
    }
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 14 || pin > 19) {
    Serial.println(F("Use A0-A5 for analog reading"));
    return;
  }
  
  int raw = analogRead(pin - 14);
  float voltage = raw * (5.0 / 1023.0);
  Serial.print(F("A")); Serial.print(pin - 14);
  Serial.print(F(" = ")); Serial.print(raw);
  Serial.print(F(" (")); Serial.print(voltage, 2); Serial.println(F("V)"));
}

void cmdPWM(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: pwm <pin> <value>"));
    Serial.println(F("PWM pins: 3, 5, 6, 9, 10, 11"));
    Serial.println(F("Value: 0-255 (0-100%)"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0 || (pin != 3 && pin != 5 && pin != 6 && pin != 9 && pin != 10 && pin != 11)) {
    Serial.println(F("Invalid PWM pin. Use: 3, 5, 6, 9, 10, 11"));
    return;
  }
  
  int value = safeAtoi(argv[2]);
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  
  pinMode(pin, OUTPUT);
  analogWrite(pin, value);
  
  float percent = (value / 255.0) * 100.0;
  Serial.print(F("PWM Pin ")); Serial.print(pin);
  Serial.print(F(" = ")); Serial.print(value);
  Serial.print(F(" (")); Serial.print(percent, 0); Serial.println(F("%)"));
  
  char buf[32];
  snprintf(buf, sizeof(buf), "PWM pin %d = %d", pin, value);
  klog(buf);
}

void cmdTone(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: tone <pin> <frequency> [duration_ms]"));
    Serial.println(F("Example: tone 8 440 1000"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  int freq = safeAtoi(argv[2]);
  if (freq < 31 || freq > 65535) {
    Serial.println(F("Frequency: 31-65535 Hz"));
    return;
  }
  
  if (argc >= 4) {
    int duration = safeAtoi(argv[3]);
    tone(pin, freq, duration);
    Serial.print(F("Playing ")); Serial.print(freq);
    Serial.print(F("Hz for ")); Serial.print(duration); Serial.println(F("ms"));
  } else {
    tone(pin, freq);
    Serial.print(F("Playing ")); Serial.print(freq); Serial.println(F("Hz (continuous)"));
    Serial.println(F("Use 'notone <pin>' to stop"));
  }
}

void cmdNoTone(char** argv, uint8_t argc) {
  if (argc < 2) {
    // Stop all tones
    for (int p = 2; p <= 13; p++) noTone(p);
    Serial.println(F("All tones stopped"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  noTone(pin);
  Serial.print(F("Tone stopped on pin ")); Serial.println(pin);
}

void cmdLEDDisco(char** argv, uint8_t argc) {
  int cycles = (argc >= 2) ? safeAtoi(argv[1]) : 3;
  if (cycles <= 0) cycles = 3;
  if (cycles > 20) cycles = 20;
  
  int speed = (argc >= 3) ? safeAtoi(argv[2]) : 30;
  if (speed < 10) speed = 10;
  if (speed > 200) speed = 200;
  
  Serial.print(F("\n  \x1B[35m*** LED DISCO MODE ***\x1B[0m\n"));
  Serial.print(F("  Cycles: ")); Serial.print(cycles);
  Serial.print(F(" | Speed: ")); Serial.print(speed); Serial.println(F("ms\n"));
  
  for (int c = 0; c < cycles; c++) {
    // Knight Rider effect
    for (int p = 2; p <= 13; p++) {
      pinMode(p, OUTPUT);
      digitalWrite(p, HIGH);
      delay(speed);
      digitalWrite(p, LOW);
    }
    for (int p = 12; p >= 3; p--) {
      pinMode(p, OUTPUT);
      digitalWrite(p, HIGH);
      delay(speed);
      digitalWrite(p, LOW);
    }
    
    // Binary counter
    for (int i = 0; i < 8; i++) {
      for (int p = 2; p <= 9; p++) {
        pinMode(p, OUTPUT);
        digitalWrite(p, (i >> (p - 2)) & 1);
      }
      delay(speed * 3);
    }
    
    Serial.print(F("\r  Cycle ")); Serial.print(c + 1);
    Serial.print(F("/")); Serial.print(cycles);
  }
  
  // Cleanup
  for (int p = 2; p <= 13; p++) {
    pinMode(p, INPUT);
  }
  Serial.println(F("\n  \x1B[32mDisco Complete!\x1B[0m"));
  klog("LED disco completed");
}

void cmdSensor(char** argv, uint8_t argc) {
  Serial.println(F("\n  \x1B[33m=== Sensor Monitor ===\x1B[0m"));
  Serial.println(F("  Reading all analog inputs...\n"));
  
  for (int a = 0; a <= 5; a++) {
    // Average 5 readings
    long sum = 0;
    for (int i = 0; i < 5; i++) {
      sum += analogRead(a);
      delay(1);
    }
    int avg = sum / 5;
    float voltage = avg * (5.0 / 1023.0);
    
    // Draw bar
    int barLen = map(avg, 0, 1023, 0, 20);
    Serial.print(F("  A"));
    Serial.print(a);
    Serial.print(F(" ["));
    for (int b = 0; b < 20; b++) {
      Serial.print(b < barLen ? F("\x1B[32m█\x1B[0m") : F("\x1B[90m░\x1B[0m"));
    }
    Serial.print(F("] "));
    Serial.print(avg);
    Serial.print(F(" ("));
    Serial.print(voltage, 2);
    Serial.println(F("V)"));
  }
}

void cmdScope(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: scope <A0-A5> [samples]"));
    Serial.println(F("Simple oscilloscope - reads analog pin"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 14 || pin > 19) {
    Serial.println(F("Use A0-A5 for scope"));
    return;
  }
  
  int samples = (argc >= 3) ? safeAtoi(argv[2]) : 50;
  if (samples < 10) samples = 10;
  if (samples > 200) samples = 200;
  
  Serial.print(F("\n  \x1B[36mScope: A")); Serial.print(pin - 14);
  Serial.print(F(" | Samples: ")); Serial.print(samples);
  Serial.println(F("\x1B[0m\n"));
  
  // Collect samples
  int values[200];
  for (int i = 0; i < samples; i++) {
    values[i] = analogRead(pin - 14);
    delay(5);
  }
  
  // Find min/max
  int vmin = 1023, vmax = 0;
  for (int i = 0; i < samples; i++) {
    if (values[i] < vmin) vmin = values[i];
    if (values[i] > vmax) vmax = values[i];
  }
  
  // Draw waveform
  int height = 16;
  for (int row = height - 1; row >= 0; row--) {
    Serial.print(F("  "));
    for (int i = 0; i < samples && i < 100; i++) {
      int mapped = map(values[i], vmin, vmax, 0, height - 1);
      if (mapped == row) {
        Serial.print(F("\x1B[32m█\x1B[0m"));
      } else if (mapped > row) {
        Serial.print(F("\x1B[90m│\x1B[0m"));
      } else {
        Serial.print(' ');
      }
    }
    Serial.println();
  }
  
  Serial.print(F("  Min: ")); Serial.print(vmin);
  Serial.print(F(" | Max: ")); Serial.print(vmax);
  Serial.print(F(" | Avg: "));
  long sum = 0;
  for (int i = 0; i < samples; i++) sum += values[i];
  Serial.println(sum / samples);
}

void cmdMorse(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: morse <pin> <message>"));
    Serial.println(F("Example: morse LED SOS"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  
  const char* morseTable[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",
    ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",
    "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."
  };
  
  Serial.print(F("Morse on pin ")); Serial.print(pin); Serial.print(F(": "));
  
  for (int i = 2; i < argc; i++) {
    for (char* c = argv[i]; *c; c++) {
      char ch = *c;
      if (ch >= 'a' && ch <= 'z') ch -= 32;
      
      if (ch >= 'A' && ch <= 'Z') {
        Serial.print(ch);
        const char* code = morseTable[ch - 'A'];
        for (const char* m = code; *m; m++) {
          digitalWrite(pin, HIGH);
          Serial.print(*m == '.' ? '.' : '-');
          delay(*m == '.' ? 100 : 300);
          digitalWrite(pin, LOW);
          delay(100);
        }
        delay(300); // Between letters
      } else if (ch == ' ') {
        Serial.print(' ');
        delay(700); // Between words
      }
    }
    Serial.print(' ');
  }
  
  Serial.println(F(" Done!"));
  klog("Morse code transmitted");
}

// === File Operations ===
void cmdLS(char** argv, uint8_t argc) {
  Serial.println(F("\n  \x1B[33mName         Type  Size  Created\x1B[0m"));
  Serial.println(F("  \x1B[90m------------ ----  ----  -------\x1B[0m"));
  
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && strcmp(fs[i].parentDir, currentPath) == 0) {
      Serial.print(F("  "));
      Serial.print(fs[i].name);
      for (uint8_t s = strlen(fs[i].name); s < 13; s++) Serial.print(' ');
      Serial.print(IS_DIR(fs[i]) ? F("\x1B[34mDIR \x1B[0m ") : F("\x1B[37mFILE\x1B[0m "));
      
      if (!IS_DIR(fs[i])) {
        Serial.print(strlen(fs[i].content));
        Serial.print(F("b  "));
      } else {
        Serial.print(F(" -   "));
      }
      
      unsigned long age = (millis() - fs[i].created) / 1000;
      Serial.print(age);
      Serial.println('s');
      count++;
    }
  }
  
  if (count == 0) Serial.println(F("  (empty)"));
  Serial.print(F("  \x1B[90m")); Serial.print(count); Serial.println(F(" items\x1B[0m\n"));
}

void cmdMKDIR(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: mkdir <dirname>"));
    return;
  }
  
  int8_t slot = findFreeSlot();
  if (slot < 0) { pprintln(S_NOSPACE); return; }
  
  strncpy(fs[slot].name, argv[1], NAME_LEN - 1);
  strncpy(fs[slot].parentDir, currentPath, PATH_LEN - 1);
  fs[slot].flags = 3; // active + directory
  fs[slot].created = millis();
  fs[slot].content[0] = 0;
  
  Serial.print(F("Directory '"));
  Serial.print(argv[1]);
  Serial.println(F("' created"));
}

void cmdTOUCH(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: touch <filename>"));
    return;
  }
  
  int8_t existing = findFile(argv[1], currentPath);
  if (existing >= 0) {
    fs[existing].created = millis();
    Serial.println(F("Timestamp updated"));
    return;
  }
  
  int8_t slot = findFreeSlot();
  if (slot < 0) { pprintln(S_NOSPACE); return; }
  
  strncpy(fs[slot].name, argv[1], NAME_LEN - 1);
  strncpy(fs[slot].parentDir, currentPath, PATH_LEN - 1);
  fs[slot].flags = 1; // active, not directory
  fs[slot].created = millis();
  fs[slot].content[0] = 0;
  
  Serial.print(F("File '"));
  Serial.print(argv[1]);
  Serial.println(F("' created"));
}

void cmdCAT(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: cat <filename>"));
    return;
  }
  
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) { pprintln(S_NOT_FOUND); return; }
  if (IS_DIR(fs[idx])) {
    Serial.println(F("Is a directory"));
    return;
  }
  
  if (strlen(fs[idx].content) > 0) {
    Serial.println(fs[idx].content);
  } else {
    pprintln(S_EMPTY);
  }
}

void cmdWRITE(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: writefile <filename> <content>"));
    Serial.println(F("Use quotes for content with spaces"));
    return;
  }
  
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) {
    // Create new file
    idx = findFreeSlot();
    if (idx < 0) { pprintln(S_NOSPACE); return; }
    strncpy(fs[idx].name, argv[1], NAME_LEN - 1);
    strncpy(fs[idx].parentDir, currentPath, PATH_LEN - 1);
    fs[idx].flags = 1;
    fs[idx].created = millis();
  }
  
  strncpy(fs[idx].content, argv[2], CONTENT_LEN - 1);
  fs[idx].content[CONTENT_LEN - 1] = 0;
  
  Serial.print(F("Written to '"));
  Serial.print(argv[1]);
  Serial.println(F("'"));
}

void cmdRM(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: rm <name>"));
    return;
  }
  
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) { pprintln(S_NOT_FOUND); return; }
  
  if (IS_DIR(fs[idx])) {
    // Remove contents recursively
    char dirPath[PATH_LEN];
    snprintf(dirPath, PATH_LEN, "%s%s/", currentPath, argv[1]);
    uint8_t dlen = strlen(dirPath);
    for (uint8_t i = 0; i < MAX_FILES; i++) {
      if (IS_ACTIVE(fs[i]) && strncmp(fs[i].parentDir, dirPath, dlen) == 0) {
        fs[i].flags = 0;
      }
    }
  }
  
  fs[idx].flags = 0;
  Serial.print(F("Removed '"));
  Serial.print(argv[1]);
  Serial.println(F("'"));
}

void cmdCD(char** argv, uint8_t argc) {
  if (argc < 2 || strcmp(argv[1], "/") == 0) {
    strncpy(currentPath, "/", PATH_LEN - 1);
    return;
  }
  
  if (strcmp(argv[1], "..") == 0) {
    // Go up one level
    if (strcmp(currentPath, "/") == 0) return;
    int len = strlen(currentPath);
    if (len <= 1) {
      strncpy(currentPath, "/", PATH_LEN - 1);
      return;
    }
    // Remove last directory
    currentPath[len - 1] = 0; // Remove trailing /
    char* lastSlash = strrchr(currentPath, '/');
    if (lastSlash) {
      *(lastSlash + 1) = 0;
    } else {
      strncpy(currentPath, "/", PATH_LEN - 1);
    }
    return;
  }
  
  // Find directory
  for (uint8_t i = 0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && IS_DIR(fs[i]) && 
        strcmp(fs[i].name, argv[1]) == 0 && 
        strcmp(fs[i].parentDir, currentPath) == 0) {
      uint8_t clen = strlen(currentPath);
      uint8_t nlen = strlen(fs[i].name);
      if (clen + nlen + 2 < PATH_LEN) {
        strcat(currentPath, fs[i].name);
        strcat(currentPath, "/");
      }
      return;
    }
  }
  
  Serial.print(F("cd: '"));
  Serial.print(argv[1]);
  Serial.println(F("' not found"));
}

// === Code Execution ===
void cmdRUN(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: run <script>"));
    Serial.println(F("Script syntax: cmd1; cmd2; cmd3"));
    Serial.println(F("Example file content: gpio 13 on;delay 500;gpio 13 off"));
    return;
  }
  
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) {
    // Try running as inline code
    Serial.println(F("Script not found. Use 'eval' for inline code."));
    return;
  }
  
  if (IS_DIR(fs[idx])) {
    Serial.println(F("Cannot run a directory"));
    return;
  }
  
  Serial.print(F("\n  \x1B[36m>>> Running: "));
  Serial.print(argv[1]);
  Serial.println(F(" <<<\x1B[0m\n"));
  
  // Parse and execute script
  char script[CONTENT_LEN];
  strncpy(script, fs[idx].content, CONTENT_LEN - 1);
  
  char* cmd = strtok(script, ";");
  int lineNum = 0;
  
  while (cmd != NULL) {
    lineNum++;
    // Trim leading spaces
    while (*cmd == ' ') cmd++;
    
    if (strlen(cmd) > 0) {
      Serial.print(F("  ["));
      Serial.print(lineNum);
      Serial.print(F("] $ "));
      Serial.println(cmd);
      
      char cmdCopy[CMD_LEN];
      strncpy(cmdCopy, cmd, CMD_LEN - 1);
      executeCommand(cmdCopy);
      delay(50);
    }
    cmd = strtok(NULL, ";");
  }
  
  Serial.println(F("  \x1B[32m>>> Done <<<\x1B[0m\n"));
  klog("Script executed");
}

void cmdEVAL(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: eval \"command1; command2; ...\""));
    Serial.println(F("Run inline code directly on Arduino!"));
    Serial.println(F("Example: eval \"gpio 13 on; delay 500; gpio 13 off\""));
    return;
  }
  
  Serial.print(F("\n  \x1B[36m>>> Executing inline code <<<\x1B[0m\n\n"));
  
  // Reconstruct full code string
  char code[CONTENT_LEN] = "";
  for (uint8_t i = 1; i < argc; i++) {
    if (i > 1) strcat(code, " ");
    strncat(code, argv[i], CONTENT_LEN - strlen(code) - 1);
  }
  
  char* cmd = strtok(code, ";");
  int lineNum = 0;
  
  while (cmd != NULL) {
    lineNum++;
    while (*cmd == ' ') cmd++;
    
    if (strlen(cmd) > 0) {
      Serial.print(F("  ["));
      Serial.print(lineNum);
      Serial.print(F("] $ "));
      Serial.println(cmd);
      
      char cmdCopy[CMD_LEN];
      strncpy(cmdCopy, cmd, CMD_LEN - 1);
      executeCommand(cmdCopy);
      delay(50);
    }
    cmd = strtok(NULL, ";");
  }
  
  Serial.println(F("  \x1B[32m>>> Done <<<\x1B[0m\n"));
  klog("Inline code executed");
}

void cmdDELAY(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("Usage: delay <milliseconds>"));
    return;
  }
  
  int ms = safeAtoi(argv[1]);
  if (ms < 0) ms = 0;
  if (ms > 30000) ms = 30000; // Max 30 seconds
  
  Serial.print(F("Waiting "));
  Serial.print(ms);
  Serial.println(F("ms..."));
  delay(ms);
}

void cmdFOR(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: for <count> <command>"));
    Serial.println(F("Example: for 10 \"gpio 13 toggle; delay 200\""));
    return;
  }
  
  int count = safeAtoi(argv[1]);
  if (count <= 0) count = 1;
  if (count > 100) count = 100;
  
  char cmd[CONTENT_LEN] = "";
  for (uint8_t i = 2; i < argc; i++) {
    if (i > 2) strcat(cmd, " ");
    strncat(cmd, argv[i], CONTENT_LEN - strlen(cmd) - 1);
  }
  
  Serial.print(F("Loop "));
  Serial.print(count);
  Serial.println(F(" times:"));
  
  for (int i = 0; i < count; i++) {
    Serial.print(F("\r  ["));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.print(count);
    Serial.print(F("]"));
    
    char cmdCopy[CMD_LEN];
    strncpy(cmdCopy, cmd, CMD_LEN - 1);
    executeCommand(cmdCopy);
    delay(10);
  }
  Serial.println();
}

void cmdMONITOR(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: monitor <pin> <interval_ms> [duration_s]"));
    Serial.println(F("Example: monitor A0 500 10"));
    return;
  }
  
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) {
    Serial.println(F("Invalid pin"));
    return;
  }
  
  int interval = safeAtoi(argv[2]);
  if (interval < 100) interval = 100;
  
  int duration = (argc >= 4) ? safeAtoi(argv[3]) : 10;
  if (duration < 1) duration = 1;
  if (duration > 60) duration = 60;
  
  bool isAnalog = (pin >= 14 && pin <= 19);
  int analogChannel = isAnalog ? pin - 14 : -1;
  
  Serial.print(F("\n  \x1B[33mMonitoring "));
  Serial.print(isAnalog ? F("A") : F("D"));
  Serial.print(isAnalog ? analogChannel : pin);
  Serial.print(F(" every "));
  Serial.print(interval);
  Serial.print(F("ms for "));
  Serial.print(duration);
  Serial.println(F("s\x1B[0m\n"));
  
  unsigned long start = millis();
  unsigned long endTime = start + (duration * 1000UL);
  
  while (millis() < endTime) {
    int value;
    if (isAnalog) {
      value = analogRead(analogChannel);
    } else {
      pinMode(pin, INPUT_PULLUP);
      delay(2);
      value = digitalRead(pin);
    }
    
    unsigned long elapsed = (millis() - start) / 1000;
    Serial.print(F("  [t+"));
    Serial.print(elapsed);
    Serial.print(F("s] "));
    
    if (isAnalog) {
      Serial.print(value);
      Serial.print(F(" ("));
      Serial.print(value * 5.0 / 1023.0, 2);
      Serial.println(F("V)"));
    } else {
      Serial.println(value ? F("HIGH") : F("LOW"));
    }
    
    delay(interval);
  }
  
  Serial.println(F("  \x1B[32mMonitoring complete\x1B[0m\n"));
}

// === System Commands ===
void cmdHELP(char** argv, uint8_t argc) {
  Serial.println(F("\n  \x1B[36m╔══════════════════════════════════════╗\x1B[0m"));
  Serial.println(F("  \x1B[36m║\x1B[0m     \x1B[33mArduinOS v3.0 Commands\x1B[0m        \x1B[36m║\x1B[0m"));
  Serial.println(F("  \x1B[36m╚══════════════════════════════════════╝\x1B[0m\n"));
  
  Serial.println(F("  \x1B[32mHardware Control:\x1B[0m"));
  Serial.println(F("    pinmode <pin> <mode>     Set pin mode"));
  Serial.println(F("    write <pin> <HIGH|LOW>   Digital write"));
  Serial.println(F("    read [pin]               Digital read (all if no pin)"));
  Serial.println(F("    aread [pin]              Analog read (all if no pin)"));
  Serial.println(F("    pwm <pin> <0-255>        PWM output"));
  Serial.println(F("    tone <pin> <freq> [ms]   Play tone"));
  Serial.println(F("    notone [pin]             Stop tone"));
  Serial.println(F("    gpio <pin> <on|off|toggle> Quick GPIO"));
  Serial.println(F("    disco [cycles] [speed]   LED disco show"));
  Serial.println(F("    sensor                   Read all sensors"));
  Serial.println(F("    scope <A0-A5> [samples]  Mini oscilloscope"));
  Serial.println(F("    monitor <pin> <ms> [s]   Live pin monitoring"));
  Serial.println(F("    morse <pin> <msg>        Morse code output\n"));
  
  Serial.println(F("  \x1B[32mFile System:\x1B[0m"));
  Serial.println(F("    ls, cd <dir>, cd .., pwd"));
  Serial.println(F("    mkdir <name>, touch <name>"));
  Serial.println(F("    cat <file>, writefile <file> <content>"));
  Serial.println(F("    rm <name>\n"));
  
  Serial.println(F("  \x1B[32mProgramming:\x1B[0m"));
  Serial.println(F("    eval \"cmd1; cmd2; ...\"   Run inline code"));
  Serial.println(F("    run <script>             Execute script file"));
  Serial.println(F("    for <n> \"cmd\"            Loop command n times"));
  Serial.println(F("    delay <ms>               Wait milliseconds\n"));
  
  Serial.println(F("  \x1B[32mSystem:\x1B[0m"));
  Serial.println(F("    help, sysinfo, neofetch, uptime"));
  Serial.println(F("    dmesg, free, df, clear, reboot\n"));
  
  Serial.println(F("  \x1B[90mPins: D2-D13, A0-A5, LED, TX, RX\x1B[0m"));
  Serial.println(F("  \x1B[90mPWM pins: 3, 5, 6, 9, 10, 11\x1B[0m"));
  Serial.println(F("  \x1B[90mAnalog in: A0-A5\x1B[0m\n"));
}

void cmdSYSINFO(char** argv, uint8_t argc) {
  unsigned long uptime = (millis() - bootTime) / 1000;
  uint8_t hours = uptime / 3600;
  uint8_t mins = (uptime % 3600) / 60;
  uint8_t secs = uptime % 60;
  
  Serial.println(F("\n\
  \x1B[36m   ______      __           __\x1B[0m\n\
  \x1B[36m  /\\  _  \\    /\\ \\         /\\ \\\x1B[0m\n\
  \x1B[36m  \\ \\ \\L\\ \\   \\ \\ \\        \\ \\ \\\x1B[0m\n\
  \x1B[36m   \\ \\  __ \\   \\ \\ \\  __  __\\ \\ \\\x1B[0m\n\
  \x1B[36m    \\ \\ \\/\\ \\   \\ \\ \\_\\ \\/\\ \\_\\ \\ \\____\x1B[0m\n\
  \x1B[36m     \\ \\_\\ \\_\\   \\ \\____/\\ \\____/\\ \\____\x1B[0m\n\
  \x1B[36m      \\/_/\\/_/    \\/___/  \\/___/  \\/____/\x1B[0m\n"));
  
  Serial.print(F("  \x1B[33mOS:\x1B[0m ArduinOS v3.0 KernelUNO\n"));
  Serial.print(F("  \x1B[33mHost:\x1B[0m Arduino UNO R3\n"));
  Serial.print(F("  \x1B[33mCPU:\x1B[0m ATmega328P @ 16MHz\n"));
  Serial.print(F("  \x1B[33mRAM:\x1B[0m ")); Serial.print(freeRAM()); Serial.println(F("/2048 bytes"));
  Serial.print(F("  \x1B[33mUptime:\x1B[0m ")); 
  Serial.print(hours); Serial.print('h ');
  Serial.print(mins); Serial.print('m ');
  Serial.print(secs); Serial.println('s');
  
  uint8_t fileCount = 0;
  for (uint8_t i = 0; i < MAX_FILES; i++) if (IS_ACTIVE(fs[i])) fileCount++;
  Serial.print(F("  \x1B[33mFiles:\x1B[0m ")); Serial.print(fileCount);
  Serial.print(F("/")); Serial.println(MAX_FILES);
}

void cmdDMESG(char** argv, uint8_t argc) {
  Serial.println(F("\n  \x1B[33m=== Kernel Message Log ===\x1B[0m\n"));
  for (uint8_t i = 0; i < dmesgCount; i++) {
    uint8_t idx = (dmesgIndex - 1 - i + DMESG_LINES) % DMESG_LINES;
    if (dmesg[idx].message[0]) {
      Serial.print(F("  \x1B[90m["));
      Serial.print(dmesg[idx].timestamp);
      Serial.print(F("s]\x1B[0m "));
      Serial.println(dmesg[idx].message);
    }
  }
  Serial.println();
}

// === Main Command Router ===
void executeCommand(char* line) {
  // Skip empty lines and comments
  while (*line == ' ') line++;
  if (strlen(line) == 0 || line[0] == '#') return;
  
  // Parse arguments
  char** argv = args;
  uint8_t argc = 0;
  parseCommand(line, argv, &argc);
  
  if (argc == 0) return;
  
  // Convert command to lowercase
  strlower(argv[0]);
  
  // === Hardware Commands ===
  if (strcmp(argv[0], "pinmode") == 0) cmdPinMode(argv, argc);
  else if (strcmp(argv[0], "write") == 0 || strcmp(argv[0], "digitalwrite") == 0) cmdDigitalWrite(argv, argc);
  else if (strcmp(argv[0], "read") == 0 || strcmp(argv[0], "digitalread") == 0) cmdDigitalRead(argv, argc);
  else if (strcmp(argv[0], "aread") == 0 || strcmp(argv[0], "analogread") == 0) cmdAnalogRead(argv, argc);
  else if (strcmp(argv[0], "pwm") == 0 || strcmp(argv[0], "analogwrite") == 0) cmdPWM(argv, argc);
  else if (strcmp(argv[0], "tone") == 0) cmdTone(argv, argc);
  else if (strcmp(argv[0], "notone") == 0) cmdNoTone(argv, argc);
  else if (strcmp(argv[0], "gpio") == 0) {
    // Route to appropriate handler based on args
    if (argc >= 2 && strcmp(argv[1], "vixa") == 0) cmdLEDDisco(argv, argc);
    else if (argc >= 3) cmdDigitalWrite(argv, argc);
    else Serial.println(F("Usage: gpio <pin> <on|off|toggle> | gpio vixa [cycles] [speed]"));
  }
  else if (strcmp(argv[0], "disco") == 0) cmdLEDDisco(argv, argc);
  else if (strcmp(argv[0], "sensor") == 0) cmdSensor(argv, argc);
  else if (strcmp(argv[0], "scope") == 0) cmdScope(argv, argc);
  else if (strcmp(argv[0], "morse") == 0) cmdMorse(argv, argc);
  else if (strcmp(argv[0], "monitor") == 0) cmdMONITOR(argv, argc);
  
  // === Programming Commands ===
  else if (strcmp(argv[0], "eval") == 0 || strcmp(argv[0], "exec") == 0) cmdEVAL(argv, argc);
  else if (strcmp(argv[0], "run") == 0 || strcmp(argv[0], "sh") == 0) cmdRUN(argv, argc);
  else if (strcmp(argv[0], "delay") == 0 || strcmp(argv[0], "sleep") == 0) cmdDELAY(argv, argc);
  else if (strcmp(argv[0], "for") == 0 || strcmp(argv[0], "loop") == 0) cmdFOR(argv, argc);
  
  // === File Commands ===
  else if (strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "dir") == 0) cmdLS(argv, argc);
  else if (strcmp(argv[0], "cd") == 0) cmdCD(argv, argc);
  else if (strcmp(argv[0], "pwd") == 0) Serial.println(currentPath);
  else if (strcmp(argv[0], "mkdir") == 0) cmdMKDIR(argv, argc);
  else if (strcmp(argv[0], "touch") == 0 || strcmp(argv[0], "create") == 0) cmdTOUCH(argv, argc);
  else if (strcmp(argv[0], "cat") == 0 || strcmp(argv[0], "type") == 0) cmdCAT(argv, argc);
  else if (strcmp(argv[0], "writefile") == 0 || strcmp(argv[0], "echo") == 0) {
    // Handle echo with redirection
    if (argc >= 3 && strcmp(argv[argc-2], ">") == 0) {
      cmdWRITE(argv, argc);
    } else if (argc >= 2) {
      // Simple echo
      for (uint8_t i = 1; i < argc; i++) {
        Serial.print(argv[i]);
        if (i < argc - 1) Serial.print(' ');
      }
      Serial.println();
    }
  }
  else if (strcmp(argv[0], "rm") == 0 || strcmp(argv[0], "del") == 0) cmdRM(argv, argc);
  
  // === System Commands ===
  else if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) cmdHELP(argv, argc);
  else if (strcmp(argv[0], "sysinfo") == 0 || strcmp(argv[0], "neofetch") == 0) cmdSYSINFO(argv, argc);
  else if (strcmp(argv[0], "dmesg") == 0 || strcmp(argv[0], "log") == 0) cmdDMESG(argv, argc);
  else if (strcmp(argv[0], "uptime") == 0) {
    unsigned long t = (millis() - bootTime) / 1000;
    Serial.print(t / 3600); Serial.print('h ');
    Serial.print((t % 3600) / 60); Serial.print('m ');
    Serial.print(t % 60); Serial.println('s');
  }
  else if (strcmp(argv[0], "free") == 0 || strcmp(argv[0], "df") == 0) {
    Serial.print(F("RAM: ")); Serial.print(freeRAM()); Serial.println(F(" bytes free"));
    uint8_t used = 0;
    for (uint8_t i = 0; i < MAX_FILES; i++) if (IS_ACTIVE(fs[i])) used++;
    Serial.print(F("Files: ")); Serial.print(used);
    Serial.print('/'); Serial.println(MAX_FILES);
  }
  else if (strcmp(argv[0], "whoami") == 0) Serial.println(F("root"));
  else if (strcmp(argv[0], "uname") == 0) Serial.println(F("ArduinOS v3.0 KernelUNO avr"));
  else if (strcmp(argv[0], "clear") == 0 || strcmp(argv[0], "cls") == 0) {
    for (uint8_t i = 0; i < 30; i++) Serial.println();
    showLogo();
  }
  else if (strcmp(argv[0], "reboot") == 0) {
    Serial.println(F("\n  Rebooting...\n"));
    delay(500);
    softReset();
  }
  else if (strcmp(argv[0], "wave") == 0) drawWave();
  
  else {
    Serial.print(F("'"));
    Serial.print(argv[0]);
    Serial.println(F("' not found. Type 'help'"));
  }
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  bootTime = millis();
  
  // Initialize hardware
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Boot sequence
  for (uint8_t i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  
  Serial.println(F("\x1B[2J\x1B[H")); // Clear screen
  showLogo();
  
  initFileSystem();
  klog("Kernel v3.0 booted");
  klog("Hardware initialized");
  
  printPrompt();
}

// === Prompt ===
void printPrompt() {
  Serial.print(F("\x1B[32mroot@arduinos\x1B[0m:\x1B[34m"));
  Serial.print(currentPath);
  Serial.print(F("\x1B[0m$ "));
}

// === Main Loop ===
void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\r' || c == '\n') {
      if (inputLen > 0) {
        inputBuffer[inputLen] = 0;
        Serial.println();
        executeCommand(inputBuffer);
        inputLen = 0;
        memset(inputBuffer, 0, CMD_LEN);
        printPrompt();
      }
    } else if (c == 8 || c == 127) {
      if (inputLen > 0) {
        inputLen--;
        Serial.print(F("\b \b"));
      }
    } else if (c == '\t') {
      // Tab completion stub
      Serial.print(F("\x1B[90m...\x1B[0m"));
    } else if (inputLen < CMD_LEN - 1 && c >= 32 && c < 127) {
      Serial.print(c);
      inputBuffer[inputLen++] = c;
    }
  }
}