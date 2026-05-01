/*
 * ArduinOS v1.0.3 - Main sketch
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
  uint8_t flags;      // bit0=active, bit1=isDir, bit2=system
  unsigned long created;
} RAMFile;

#define IS_ACTIVE(f)  (f.flags & 1)
#define IS_DIR(f)     (f.flags & 2)
#define IS_SYSTEM(f)  (f.flags & 4)

// Kernel Log Structure
typedef struct {
  unsigned long timestamp;
  char message[DMESG_LEN];
} DmesgEntry;

// Global State
RAMFile fs[MAX_FILES];
char currentPath[PATH_LEN] = "/";
char inputBuffer[CMD_LEN];
int8_t inputLen = 0;
DmesgEntry dmesg[DMESG_LINES];
uint8_t dmesgIndex = 0, dmesgCount = 0;
unsigned long bootTime;
char* args[MAX_ARGS];
uint8_t argCount = 0;

// Soft reset function pointer (moved here from utils.ino)
void(* softReset)(void) = 0;

// Forward declarations
void executeCommand(char* line);
void printPrompt();
void initFileSystem();
void klog(const char* msg);

void setup() {
  Serial.begin(115200);
  bootTime = millis();
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  Serial.print(F("\x1B[2J\x1B[H"));
  Serial.println(F("ArduinOS v1.0 ready."));
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
    } else if (inputLen < CMD_LEN - 1 && c >= 32) {
      Serial.print(c);
      inputBuffer[inputLen++] = c;
    }
  }
}