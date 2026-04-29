/*
 * ArduinOS v1.0.2 - Kernel
 * GPIO, Analog, PWM control
 * Fixed: writefile, gpio toggle, eval/for quoting, uptime format
 */
 
#include <Arduino.h>
#include <string.h>
#include <avr/pgmspace.h>

// Configuration
#define MAX_FILES 8
#define CONTENT_LEN 24
#define PATH_LEN 16
#define DMESG_LINES 3
#define DMESG_LEN 24
#define CMD_LEN 32
#define NAME_LEN 10
#define MAX_ARGS 4

// File Structure
typedef struct {
  char name[NAME_LEN];
  char content[CONTENT_LEN];
  char parentDir[PATH_LEN];
  uint8_t flags; // bit0active, bit1isDir, bit2system
  unsigned long created;
} RAMFile;

#define IS_ACTIVE(f) (f.flags & 1)
#define IS_DIR(f) (f.flags & 2)
#define IS_SYSTEM(f) (f.flags & 4)

// Kernel Log Structure
typedef struct {
  unsigned long timestamp;
  char message[DMESG_LEN];
} DmesgEntry;

//  Global State 
RAMFile fs[MAX_FILES];
char currentPath[PATH_LEN]  "/";
char inputBuffer[CMD_LEN];
int8_t inputLen  0;
DmesgEntry dmesg[DMESG_LINES];
uint8_t dmesgIndex  0, dmesgCount  0;
unsigned long bootTime;
char* args[MAX_ARGS];
uint8_t argCount  0;

//  PROGMEM helpers 
void pprint(const char* pgm_str) {
  char c;
  while ((c  pgm_read_byte(pgm_str++))) Serial.print(c);
}
void pprintln(const char* pgm_str) {
  pprint(pgm_str);
  Serial.println();
}

//  Kernel Logging 
void klog(const char* msg) {
  uint8_t idx  dmesgIndex % DMESG_LINES;
  dmesg[idx].timestamp  (millis() - bootTime) / 1000;
  strncpy(dmesg[idx].message, msg, DMESG_LEN - 1);
  dmesg[idx].message[DMESG_LEN - 1]  0;
  dmesgIndex++;
  if (dmesgCount < DMESG_LINES) dmesgCount++;
}

//  Soft Reset 
void(* softReset)(void)  0;

//  Free RAM 
int freeRAM() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval  0 ? (int)&__heap_start : (int)__brkval);
}

//  String Utilities 
int safeAtoi(const char* str) {
  int num  0, sign  1;
  if (*str  '-') { sign  -1; str++; }
  while (*str > '0' && *str < '9') { num  num * 10 + (*str - '0'); str++; }
  return num * sign;
}

void strlower(char* str) {
  for (uint8_t i  0; str[i]; i++)
    if (str[i] > 'A' && str[i] < 'Z') str[i] + 32;
}

// Strip surrounding double quotes (in-place)
void stripQuotes(char* str) {
  int len  strlen(str);
  if (len > 2 && str[0]  '"' && str[len-1]  '"') {
    memmove(str, str+1, len-2);
    str[len-2]  0;
  }
}

//  Command Parser 
void parseCommand(char* line, char** argv, uint8_t* argc) {
  *argc  0;
  char* token  strtok(line, " ");
  while (token ! NULL && *argc < MAX_ARGS) {
    argv[(*argc)++]  token;
    token  strtok(NULL, " ");
  }
}

//  Filesystem 
int8_t findFile(const char* name, const char* path) {
  for (uint8_t i  0; i < MAX_FILES; i++)
    if (IS_ACTIVE(fs[i]) && !strcmp(fs[i].name, name) && !strcmp(fs[i].parentDir, path))
      return i;
  return -1;
}

int8_t findFreeSlot() {
  for (uint8_t i  0; i < MAX_FILES; i++)
    if (!IS_ACTIVE(fs[i])) return i;
  return -1;
}

void initFileSystem() {
  memset(fs, 0, sizeof(fs));
  const char* dirs[]  {"home", "dev", "tmp", "etc"};
  for (uint8_t d  0; d < 4; d++) {
    int8_t slot  findFreeSlot();
    if (slot > 0) {
      strncpy(fs[slot].name, dirs[d], NAME_LEN - 1);
      strncpy(fs[slot].parentDir, "/", PATH_LEN - 1);
      fs[slot].flags  3; // active + directory
      fs[slot].created  millis();
    }
  }
  klog("Filesystem initialized");
}

//  Pin Resolution 
int8_t resolvePin(const char* name) {
  if ((name[0]  'D' || name[0]  'd') && name[1] > '2' && name[1] < '9')
    return name[1] - '0';
  if (name[0] > '0' && name[0] < '9') {
    int p  safeAtoi(name);
    if (p > 2 && p < 13) return p;
  }
  if ((name[0]  'A' || name[0]  'a') && name[1] > '0' && name[1] < '5')
    return 14 + (name[1] - '0');
  if (!strcmp(name, "LED") || !strcmp(name, "led")) return LED_BUILTIN;
  if (!strcmp(name, "TX") || !strcmp(name, "tx")) return 1;
  if (!strcmp(name, "RX") || !strcmp(name, "rx")) return 0;
  return -1;
}

//  Logo & Wave 
void showLogo() {
  Serial.print(F("\033[36m"));
  Serial.println(F("   |------------------------------|"));
  Serial.println(F("   |   \033[32mArduinOS v1.0\033[36m|"));
  Serial.println(F("   |   \033[33mKernel\033[36m     |"));
  Serial.println(F("   |------------------------------|"));
  Serial.print(F("\033[90m"));
  Serial.println(F("   ┌────────────────────────────┐"));
  Serial.print(F("   │  \033[37mCPU:\033[0m ATmega328P @ 16MHz  \033[90m│\033[0m\n"));
  Serial.print(F("   │  \033[37mRAM:\033[0m ")); Serial.print(freeRAM()); Serial.println(F(" bytes free     \033[90m│\033[0m"));
  Serial.println(F("   └────────────────────────────┘\033[0m"));
  Serial.println(F("  Type \033[33m'help'\033[0m for commands\n"));
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

//  Hardware Commands 
void cmdPinMode(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: pinmode <pin> <mode>"));
    return;
  }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  strlower(argv[2]);
  if (!strcmp(argv[2], "output") || !strcmp(argv[2], "out")) {
    pinMode(pin, OUTPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> OUTPUT"));
  } else if (!strcmp(argv[2], "input_pullup") || !strcmp(argv[2], "pullup")) {
    pinMode(pin, INPUT_PULLUP);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT_PULLUP"));
  } else if (!strcmp(argv[2], "pwm")) {
    if (pin  3 || pin  5 || pin  6 || pin  9 || pin  10 || pin  11) {
      pinMode(pin, OUTPUT);
      Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> PWM ready"));
    } else Serial.println(F("PWM only on pins 3,5,6,9,10,11"));
  } else {
    pinMode(pin, INPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT"));
  }
}

void cmdDigitalWrite(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: write/gpio <pin> <HIGH|LOW|ON|OFF|TOGGLE>"));
    return;
  }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  strlower(argv[2]);
  int value;
  if (!strcmp(argv[2], "high") || !strcmp(argv[2], "1") || !strcmp(argv[2], "on"))
    value  HIGH;
  else if (!strcmp(argv[2], "low") || !strcmp(argv[2], "0") || !strcmp(argv[2], "off"))
    value  LOW;
  else if (!strcmp(argv[2], "toggle")) {
    pinMode(pin, OUTPUT); // ensure we can read current state
    int current  digitalRead(pin);
    value  !current;
    Serial.print(F("Toggle pin ")); Serial.print(pin);
    Serial.print(F(" -> ")); Serial.println(value ? F("HIGH") : F("LOW"));
  } else {
    Serial.println(F("Invalid value. Use HIGH/LOW/ON/OFF/TOGGLE"));
    return;
  }
  pinMode(pin, OUTPUT);
  digitalWrite(pin, value);
  if (strcmp(argv[2], "toggle")) {
    Serial.print(F("Pin ")); Serial.print(pin);
    Serial.print(F("  ")); Serial.println(value ? F("HIGH") : F("LOW"));
  }
}

void cmdDigitalRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("\n  \x1B[33m Digital Pin States \x1B[0m"));
    for (int p  2; p < 13; p++) {
      pinMode(p, INPUT_PULLUP);
      delay(2);
      Serial.print(F("  ")); if (p<10) Serial.print(' ');
      Serial.print(p); Serial.print(F("  |  "));
      Serial.println(digitalRead(p) ? F("HIGH ") : F("LOW  "));
    }
    return;
  }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  pinMode(pin, INPUT_PULLUP);
  delay(5);
  Serial.print(F("Pin ")); Serial.print(pin);
  Serial.print(F("  ")); Serial.println(digitalRead(pin) ? F("HIGH") : F("LOW"));
}

void cmdAnalogRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("\n  \x1B[33m Analog Inputs \x1B[0m"));
    for (int a  0; a < 5; a++) {
      int raw  analogRead(a);
      Serial.print(F("  A")); Serial.print(a); Serial.print(F(" | "));
      Serial.print(raw); Serial.print(F(" | ")); Serial.print(raw * 5.0 / 1023.0, 2); Serial.println('V');
    }
    return;
  }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 14 || pin > 19) { Serial.println(F("Use A0-A5")); return; }
  int raw  analogRead(pin - 14);
  Serial.print(F("A")); Serial.print(pin - 14);
  Serial.print(F("  ")); Serial.print(raw);
  Serial.print(F(" (")); Serial.print(raw * 5.0 / 1023.0, 2); Serial.println(F("V)"));
}

void cmdPWM(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: pwm <pin> <0-255>"));
    return;
  }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0 || (pin ! 3 && pin ! 5 && pin ! 6 && pin ! 9 && pin ! 10 && pin ! 11)) {
    Serial.println(F("PWM pins: 3,5,6,9,10,11"));
    return;
  }
  int val  constrain(safeAtoi(argv[2]), 0, 255);
  analogWrite(pin, val);
  Serial.print(F("PWM ")); Serial.print(pin); Serial.print(F("  ")); Serial.print(val);
  Serial.print(F(" (")); Serial.print(map(val,0,255,0,100)); Serial.println(F("%)"));
}

void cmdTone(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: tone <pin> <freq> [ms]")); return; }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  int freq  safeAtoi(argv[2]);
  if (argc > 4) tone(pin, freq, safeAtoi(argv[3]));
  else tone(pin, freq);
  Serial.println(F("Done"));
}

void cmdNoTone(char** argv, uint8_t argc) {
  if (argc < 2) {
    for (int p  2; p < 13; p++) noTone(p);
    Serial.println(F("All tones stopped"));
  } else {
    int8_t pin  resolvePin(argv[1]);
    if (pin > 0) noTone(pin);
  }
}

void cmdLEDDisco(char** argv, uint8_t argc) {
  int cycles  (argc > 2) ? safeAtoi(argv[1]) : 3;
  if (cycles > 20) cycles  20;
  int speed  (argc > 3) ? safeAtoi(argv[2]) : 30;
  if (speed < 10) speed  10;
  Serial.print(F("\n  \x1B[35m*** DISCO ***\x1B[0m\n"));
  for (int c  0; c < cycles; c++) {
    for (int p  2; p < 13; p++) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); delay(speed); digitalWrite(p, LOW); }
    for (int p  12; p > 3; p--) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); delay(speed); digitalWrite(p, LOW); }
    for (int i  0; i < 8; i++) { for (int p  2; p < 9; p++) { pinMode(p, OUTPUT); digitalWrite(p, (i>>(p-2))&1); } delay(speed*3); }
  }
  // Reset pins to INPUT_PULLUP to avoid floating
  for (int p  2; p < 13; p++) pinMode(p, INPUT_PULLUP);
  Serial.println(F("\n  Done"));
}

void cmdSensor() {
  Serial.println(F("\n  \x1B[33m Sensor Monitor \x1B[0m"));
  for (int a  0; a < 5; a++) {
    long sum  0;
    for (int i  0; i < 5; i++) sum + analogRead(a);
    int avg  sum / 5;
    int barLen  map(avg, 0, 1023, 0, 20);
    Serial.print(F("  A")); Serial.print(a); Serial.print(F(" ["));
    for (int b  0; b < 20; b++) Serial.print(b < barLen ? F("\x1B[32m█\x1B[0m") : F("\x1B[90m░\x1B[0m"));
    Serial.print(F("] ")); Serial.print(avg); Serial.print(F(" (")); Serial.print(avg * 5.0 / 1023.0, 2); Serial.println(F("V)"));
  }
}

void cmdScope(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: scope <A0-A5> [samples]")); return; }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 14) { Serial.println(F("Use A0-A5")); return; }
  int samples  (argc > 3) ? safeAtoi(argv[2]) : 50;
  samples  constrain(samples, 10, 100);
  int values[100];
  for (int i  0; i < samples; i++) { values[i]  analogRead(pin - 14); delay(5); }
  int vmin  1023, vmax  0;
  for (int i  0; i < samples; i++) {
    if (values[i] < vmin) vmin  values[i];
    if (values[i] > vmax) vmax  values[i];
  }
  Serial.print(F("\n  Scope A")); Serial.print(pin-14); Serial.println();
  for (int row  15; row > 0; row--) {
    Serial.print(F("  "));
    for (int i  0; i < samples; i++) {
      int mapped  map(values[i], vmin, vmax, 0, 15);
      // Fixed: all branches now FlashStringHelper*
      Serial.print(mapped  row ? F("\x1B[32m█\x1B[0m") : (mapped > row ? F("\x1B[90m│\x1B[0m") : F(" ")));
    }
    Serial.println();
  }
}

void cmdMorse(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: morse <pin> <msg>")); return; }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) return;
  pinMode(pin, OUTPUT);
  const char* morseTable[]  {".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
  for (int i  2; i < argc; i++) {
    for (char* c  argv[i]; *c; c++) {
      char ch  *c;
      if (ch > 'a' && ch < 'z') ch - 32;
      if (ch > 'A' && ch < 'Z') {
        const char* code  morseTable[ch - 'A'];
        for (const char* m  code; *m; m++) {
          digitalWrite(pin, HIGH); delay(*m  '.' ? 100 : 300);
          digitalWrite(pin, LOW); delay(100);
        }
        delay(300);
      } else if (ch  ' ') delay(700);
    }
  }
  Serial.println(F(" Done"));
}

//  File Commands 
void cmdLS() {
  Serial.println(F("\n  \x1B[33mName         Type  Size  Created\x1B[0m"));
  uint8_t cnt  0;
  for (uint8_t i  0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && !strcmp(fs[i].parentDir, currentPath)) {
      Serial.print(F("  ")); Serial.print(fs[i].name);
      for (uint8_t s  strlen(fs[i].name); s < 13; s++) Serial.print(' ');
      Serial.print(IS_DIR(fs[i]) ? F("\x1B[34mDIR \x1B[0m ") : F("\x1B[37mFILE\x1B[0m "));
      if (!IS_DIR(fs[i])) { Serial.print(strlen(fs[i].content)); Serial.print(F("b  ")); }
      else Serial.print(F(" -   "));
      unsigned long age  (millis() - fs[i].created) / 1000;
      Serial.print(age); Serial.println('s');
      cnt++;
    }
  }
  if (cnt  0) Serial.println(F("  (empty)"));
  Serial.print(F("  ")); Serial.print(cnt); Serial.println(F(" items\n"));
}

void cmdMKDIR(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: mkdir <name>")); return; }
  int8_t slot  findFreeSlot();
  if (slot < 0) { Serial.println(F("No space")); return; }
  strncpy(fs[slot].name, argv[1], NAME_LEN - 1);
  strncpy(fs[slot].parentDir, currentPath, PATH_LEN - 1);
  fs[slot].flags  3;
  fs[slot].created  millis();
  Serial.print(F("Dir ")); Serial.print(argv[1]); Serial.println(F(" created"));
}

void cmdTOUCH(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: touch <file>")); return; }
  int8_t idx  findFile(argv[1], currentPath);
  if (idx > 0) { fs[idx].created  millis(); Serial.println(F("Updated")); return; }
  idx  findFreeSlot();
  if (idx < 0) { Serial.println(F("No space")); return; }
  strncpy(fs[idx].name, argv[1], NAME_LEN - 1);
  strncpy(fs[idx].parentDir, currentPath, PATH_LEN - 1);
  fs[idx].flags  1;
  fs[idx].created  millis();
  fs[idx].content[0]  0;
  Serial.print(F("File ")); Serial.print(argv[1]); Serial.println(F(" created"));
}

void cmdCAT(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: cat <file>")); return; }
  int8_t idx  findFile(argv[1], currentPath);
  if (idx < 0) { Serial.println(F("Not found")); return; }
  if (IS_DIR(fs[idx])) { Serial.println(F("Is a directory")); return; }
  if (strlen(fs[idx].content) > 0) {
    // Convert stored \n back to newline for display
    for (char* c  fs[idx].content; *c; c++) {
      if (*c  '\\' && *(c+1)  'n') { Serial.println(); c++; }
      else Serial.print(*c);
    }
    Serial.println();
  } else Serial.println(F("(empty)"));
}

void cmdWRITE(char** argv, uint8_t argc) {
  if (argc < 3) {
    Serial.println(F("Usage: writefile <file> <content>"));
    return;
  }
  // Rebuild content string from argv[2]..argv[argc-1]
  char content[CONTENT_LEN]  "";
  for (int i  2; i < argc; i++) {
    if (i > 2) strncat(content, " ", CONTENT_LEN - strlen(content) - 1);
    strncat(content, argv[i], CONTENT_LEN - strlen(content) - 1);
  }
  // Strip surrounding quotes
  stripQuotes(content);

  int8_t idx  findFile(argv[1], currentPath);
  if (idx < 0) {
    idx  findFreeSlot();
    if (idx < 0) { Serial.println(F("No space")); return; }
    strncpy(fs[idx].name, argv[1], NAME_LEN - 1);
    strncpy(fs[idx].parentDir, currentPath, PATH_LEN - 1);
    fs[idx].flags  1;
    fs[idx].created  millis();
  }
  // Replace literal \n sequences with real newline
  char finalContent[CONTENT_LEN];
  char* dst  finalContent;
  for (char* src  content; *src && (dst - finalContent) < CONTENT_LEN - 1; src++) {
    if (*src  '\\' && *(src+1)  'n') {
      *dst++  '\n';
      src++;
    } else {
      *dst++  *src;
    }
  }
  *dst  0;
  strncpy(fs[idx].content, finalContent, CONTENT_LEN - 1);
  fs[idx].content[CONTENT_LEN - 1]  0;
  Serial.print(F("Written to ")); Serial.println(argv[1]);
}

void cmdRM(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: rm <name>")); return; }
  int8_t idx  findFile(argv[1], currentPath);
  if (idx < 0) { Serial.println(F("Not found")); return; }
  if (IS_DIR(fs[idx])) {
    char dirPath[PATH_LEN];
    snprintf(dirPath, PATH_LEN, "%s%s/", currentPath, argv[1]);
    for (int i  0; i < MAX_FILES; i++)
      if (IS_ACTIVE(fs[i]) && !strncmp(fs[i].parentDir, dirPath, strlen(dirPath))) fs[i].flags  0;
  }
  fs[idx].flags  0;
  Serial.println(F("Removed"));
}

void cmdCD(char** argv, uint8_t argc) {
  if (argc < 2 || !strcmp(argv[1], "/")) {
    strncpy(currentPath, "/", PATH_LEN - 1);
    return;
  }
  if (!strcmp(argv[1], "..")) {
    if (!strcmp(currentPath, "/")) return;
    int len  strlen(currentPath);
    if (len < 1) { strncpy(currentPath, "/", PATH_LEN - 1); return; }
    currentPath[len - 1]  0; // remove trailing /
    char* lastSlash  strrchr(currentPath, '/');
    if (lastSlash) *(lastSlash + 1)  0;
    else strncpy(currentPath, "/", PATH_LEN - 1);
    return;
  }
  // Check if directory exists
  for (int i  0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && IS_DIR(fs[i]) && !strcmp(fs[i].name, argv[1]) && !strcmp(fs[i].parentDir, currentPath)) {
      if (strlen(currentPath) + strlen(fs[i].name) + 1 > PATH_LEN) { Serial.println(F("Path too long")); return; }
      strcat(currentPath, fs[i].name);
      strcat(currentPath, "/");
      return;
    }
  }
  Serial.print(F("cd: ")); Serial.print(argv[1]); Serial.println(F(" not found"));
}

//  Code Execution 
void executeCommand(char* line); // forward

void cmdRUN(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: run <script>")); return; }
  int8_t idx  findFile(argv[1], currentPath);
  if (idx < 0 || IS_DIR(fs[idx])) { Serial.println(F("Not a script")); return; }
  Serial.print(F("\n  \x1B[36m>>> Running ")); Serial.print(argv[1]); Serial.println(F(" <<<\x1B[0m"));
  char script[CONTENT_LEN];
  strncpy(script, fs[idx].content, CONTENT_LEN - 1);
  // Split by actual newline or semicolon? Use semicolon for inline, newline for files.
  // Here we use newline delimiter for scripts
  char* line  strtok(script, "\n");
  int n  0;
  while (line) {
    n++; Serial.print(F("  [")); Serial.print(n); Serial.print(F("] $ ")); Serial.println(line);
    executeCommand(line);
    line  strtok(NULL, "\n");
    delay(50);
  }
  Serial.println(F("  \x1B[32m>>> Done <<<\x1B[0m\n"));
}

void cmdEVAL(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: eval \"cmd1; cmd2; ...\"")); return; }
  // Reconstruct full code
  char code[CONTENT_LEN]  "";
  for (int i  1; i < argc; i++) {
    if (i > 1) strncat(code, " ", CONTENT_LEN - strlen(code) - 1);
    strncat(code, argv[i], CONTENT_LEN - strlen(code) - 1);
  }
  stripQuotes(code); // remove outer quotes if present
  Serial.print(F("\n  \x1B[36m>>> Eval <<<\x1B[0m\n"));
  // Split by semicolons
  char* cmd  strtok(code, ";");
  int n  0;
  while (cmd) {
    // Trim leading spaces
    while (*cmd  ' ') cmd++;
    // Strip quotes from this sub-command
    stripQuotes(cmd);
    n++; Serial.print(F("  [")); Serial.print(n); Serial.print(F("] $ ")); Serial.println(cmd);
    executeCommand(cmd);
    cmd  strtok(NULL, ";");
    delay(50);
  }
  Serial.println(F("  \x1B[32m>>> Done <<<\x1B[0m\n"));
}

void cmdDELAY(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: delay <ms>")); return; }
  int ms  safeAtoi(argv[1]);
  if (ms > 30000) ms  30000;
  delay(ms);
}

void cmdFOR(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: for <count> \"command\"")); return; }
  int count  safeAtoi(argv[1]);
  if (count > 100) count  100;
  // Rebuild command string
  char cmd[CONTENT_LEN]  "";
  for (int i  2; i < argc; i++) {
    if (i > 2) strncat(cmd, " ", CONTENT_LEN - strlen(cmd) - 1);
    strncat(cmd, argv[i], CONTENT_LEN - strlen(cmd) - 1);
  }
  stripQuotes(cmd);
  Serial.print(F("Loop ")); Serial.print(count); Serial.println(F(" times:"));
  for (int i  0; i < count; i++) {
    Serial.print(F("\r  [")); Serial.print(i+1); Serial.print('/'); Serial.print(count); Serial.print(']');
    executeCommand(cmd);
    delay(10);
  }
  Serial.println();
}

void cmdMONITOR(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: monitor <pin> <ms> [sec]")); return; }
  int8_t pin  resolvePin(argv[1]);
  if (pin < 0) return;
  int interval  safeAtoi(argv[2]);
  if (interval < 100) interval  100;
  int duration  (argc > 4) ? safeAtoi(argv[3]) : 10;
  bool isAnalog  (pin > 14);
  unsigned long stop  millis() + duration * 1000UL;
  while (millis() < stop) {
    int val  isAnalog ? analogRead(pin - 14) : (pinMode(pin, INPUT_PULLUP), digitalRead(pin));
    Serial.print(F("[t+")); Serial.print((millis() - (stop - duration*1000UL))/1000); Serial.print(F("s] "));
    if (isAnalog) Serial.print(val); else Serial.print(val ? F("HIGH") : F("LOW"));
    Serial.println();
    delay(interval);
  }
}

//  System Commands 
void cmdHELP() {
  Serial.println(F("\n  \x1B[36m╔══════════════════════════════════════╗\x1B[0m"));
  Serial.println(F("  \x1B[36m║\x1B[0m     \x1B[33mArduinOS v1.0 Commands\x1B[0m        \x1B[36m║\x1B[0m"));
  Serial.println(F("  \x1B[36m╚══════════════════════════════════════╝\x1B[0m\n"));
  Serial.println(F("  \x1B[32mHardware:\x1B[0m pinmode, write, read, aread, pwm, tone, gpio, disco, sensor, scope, morse, monitor"));
  Serial.println(F("  \x1B[32mFiles:\x1B[0m ls, cd, pwd, mkdir, touch, cat, writefile, rm"));
  Serial.println(F("  \x1B[32mCode:\x1B[0m eval, run, for, delay"));
  Serial.println(F("  \x1B[32mSystem:\x1B[0m help, sysinfo, neofetch, uptime, dmesg, free, whoami, uname, clear, reboot, wave\n"));
}

void cmdSYSINFO() {
  unsigned long sec  (millis() - bootTime) / 1000;
  uint8_t h  sec / 3600, m  (sec % 3600) / 60, s  sec % 60;
  Serial.println(F("\n\033[36m  ...\033[0m")); // ASCII logo placeholder
  Serial.print(F("  \033[33mOS:\033[0m ArduinOS v1.0\n  \033[33mHost:\033[0m Arduino UNO\n"));
  Serial.print(F("  \033[33mCPU:\033[0m ATmega328P 16MHz\n  \033[33mRAM:\033[0m "));
  Serial.print(freeRAM()); Serial.println(F("/2048 bytes"));
  Serial.print(F("  \033[33mUptime:\033[0m "));
  if (h < 10) Serial.print('0'); Serial.print(h); Serial.print(':');
  if (m < 10) Serial.print('0'); Serial.print(m); Serial.print(':');
  if (s < 10) Serial.print('0'); Serial.println(s);
}

void cmdDMESG() {
  Serial.println(F("\n  \x1B[33m Kernel Log \x1B[0m"));
  for (int i  0; i < dmesgCount; i++) {
    uint8_t idx  (dmesgIndex - 1 - i + DMESG_LINES) % DMESG_LINES;
    if (dmesg[idx].message[0]) {
      Serial.print(F("  [")); Serial.print(dmesg[idx].timestamp); Serial.print(F("s] "));
      Serial.println(dmesg[idx].message);
    }
  }
}

//  Main Command Router 
void executeCommand(char* line) {
  while (*line  ' ') line++;
  if (!*line || *line  '#') return;
  char* argv[MAX_ARGS];
  uint8_t argc;
  parseCommand(line, argv, &argc);
  if (argc  0) return;
  strlower(argv[0]);

  if (!strcmp(argv[0], "pinmode")) cmdPinMode(argv, argc);
  else if (!strcmp(argv[0], "write") || !strcmp(argv[0], "digitalwrite")) cmdDigitalWrite(argv, argc);
  else if (!strcmp(argv[0], "read") || !strcmp(argv[0], "digitalread")) cmdDigitalRead(argv, argc);
  else if (!strcmp(argv[0], "aread") || !strcmp(argv[0], "analogread")) cmdAnalogRead(argv, argc);
  else if (!strcmp(argv[0], "pwm") || !strcmp(argv[0], "analogwrite")) cmdPWM(argv, argc);
  else if (!strcmp(argv[0], "tone")) cmdTone(argv, argc);
  else if (!strcmp(argv[0], "notone")) cmdNoTone(argv, argc);
  else if (!strcmp(argv[0], "gpio")) {
    if (argc > 2 && !strcmp(argv[1], "vixa")) cmdLEDDisco(argv, argc);
    else if (argc > 3) cmdDigitalWrite(argv, argc);
    else Serial.println(F("Usage: gpio <pin> <on|off|toggle> | gpio vixa"));
  }
  else if (!strcmp(argv[0], "disco")) cmdLEDDisco(argv, argc);
  else if (!strcmp(argv[0], "sensor")) cmdSensor();
  else if (!strcmp(argv[0], "scope")) cmdScope(argv, argc);
  else if (!strcmp(argv[0], "morse")) cmdMorse(argv, argc);
  else if (!strcmp(argv[0], "monitor")) cmdMONITOR(argv, argc);
  else if (!strcmp(argv[0], "eval") || !strcmp(argv[0], "exec")) cmdEVAL(argv, argc);
  else if (!strcmp(argv[0], "run") || !strcmp(argv[0], "sh")) cmdRUN(argv, argc);
  else if (!strcmp(argv[0], "delay") || !strcmp(argv[0], "sleep")) cmdDELAY(argv, argc);
  else if (!strcmp(argv[0], "for") || !strcmp(argv[0], "loop")) cmdFOR(argv, argc);
  else if (!strcmp(argv[0], "ls") || !strcmp(argv[0], "dir")) cmdLS();
  else if (!strcmp(argv[0], "cd")) cmdCD(argv, argc);
  else if (!strcmp(argv[0], "pwd")) Serial.println(currentPath);
  else if (!strcmp(argv[0], "mkdir")) cmdMKDIR(argv, argc);
  else if (!strcmp(argv[0], "touch") || !strcmp(argv[0], "create")) cmdTOUCH(argv, argc);
  else if (!strcmp(argv[0], "cat") || !strcmp(argv[0], "type")) cmdCAT(argv, argc);
  else if (!strcmp(argv[0], "writefile")) {
    // Handle echo > redirect if needed
    if (argc > 3 && !strcmp(argv[argc-2], ">")) cmdWRITE(argv, argc);
    else cmdWRITE(argv, argc);
  }
  else if (!strcmp(argv[0], "echo")) {
    if (argc > 3 && !strcmp(argv[argc-2], ">")) cmdWRITE(argv, argc);
    else { for (int i  1; i < argc; i++) { Serial.print(argv[i]); Serial.print(' '); } Serial.println(); }
  }
  else if (!strcmp(argv[0], "rm") || !strcmp(argv[0], "del")) cmdRM(argv, argc);
  else if (!strcmp(argv[0], "help") || !strcmp(argv[0], "?")) cmdHELP();
  else if (!strcmp(argv[0], "sysinfo") || !strcmp(argv[0], "neofetch")) cmdSYSINFO();
  else if (!strcmp(argv[0], "dmesg") || !strcmp(argv[0], "log")) cmdDMESG();
  else if (!strcmp(argv[0], "uptime")) {
    unsigned long s  (millis() - bootTime) / 1000;
    uint8_t h  s/3600, m  (s%3600)/60, sec  s%60;
    if (h<10) Serial.print('0'); Serial.print(h); Serial.print(':');
    if (m<10) Serial.print('0'); Serial.print(m); Serial.print(':');
    if (sec<10) Serial.print('0'); Serial.println(sec);
  }
  else if (!strcmp(argv[0], "free") || !strcmp(argv[0], "df")) {
    Serial.print(F("RAM free: ")); Serial.print(freeRAM()); Serial.println(F(" bytes"));
    uint8_t used  0; for (int i0; i<MAX_FILES; i++) if (IS_ACTIVE(fs[i])) used++;
    Serial.print(F("Files: ")); Serial.print(used); Serial.print('/'); Serial.println(MAX_FILES);
  }
  else if (!strcmp(argv[0], "whoami")) Serial.println(F("root"));
  else if (!strcmp(argv[0], "uname")) Serial.println(F("ArduinOS v1.0 KernelUNO avr"));
  else if (!strcmp(argv[0], "clear") || !strcmp(argv[0], "cls")) { for (int i0;i<30;i++) Serial.println(); showLogo(); }
  else if (!strcmp(argv[0], "reboot")) { Serial.println(F("Rebooting...")); delay(500); softReset(); }
  else if (!strcmp(argv[0], "wave")) drawWave();
  else { Serial.print(F("Unknown: ")); Serial.println(argv[0]); }
}

void setup() {
  Serial.begin(115200);
  bootTime  millis();
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i0;i<5;i++) { digitalWrite(LED_BUILTIN, HIGH); delay(50); digitalWrite(LED_BUILTIN, LOW); delay(50); }
  Serial.print(F("\x1B[2J\x1B[H"));
  showLogo();
  initFileSystem();
  klog("Kernel v1.0 booted");
  printPrompt();
}

void printPrompt() {
  Serial.print(F("\x1B[32mroot@arduinos\x1B[0m:\x1B[34m"));
  Serial.print(currentPath);
  Serial.print(F("\x1B[0m$ "));
}

void loop() {
  while (Serial.available()) {
    char c  Serial.read();
    if (c  '\r' || c  '\n') {
      if (inputLen > 0) {
        inputBuffer[inputLen]  0;
        Serial.println();
        executeCommand(inputBuffer);
        inputLen  0;
        memset(inputBuffer, 0, CMD_LEN);
        printPrompt();
      }
    } else if (c  8 || c  127) {
      if (inputLen > 0) { inputLen--; Serial.print(F("\b \b")); }
    } else if (inputLen < CMD_LEN - 1 && c > 32) {
      Serial.print(c);
      inputBuffer[inputLen++]  c;
    }
  }
}
