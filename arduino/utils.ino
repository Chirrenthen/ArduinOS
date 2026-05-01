// PROGMEM helpers
void pprint(const char* pgm_str) {
  char c;
  while ((c = pgm_read_byte(pgm_str++))) Serial.print(c);
}

void pprintln(const char* pgm_str) {
  pprint(pgm_str);
  Serial.println();
}

void klog(const char* msg) {
  uint8_t idx = dmesgIndex % DMESG_LINES;
  dmesg[idx].timestamp = (millis() - bootTime) / 1000;
  strncpy(dmesg[idx].message, msg, DMESG_LEN - 1);
  dmesg[idx].message[DMESG_LEN - 1] = 0;
  dmesgIndex++;
  if (dmesgCount < DMESG_LINES) dmesgCount++;
}

int freeRAM() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

int safeAtoi(const char* str) {
  int num = 0, sign = 1;
  if (*str == '-') { sign = -1; str++; }
  while (*str >= '0' && *str <= '9') {
    num = num * 10 + (*str - '0');
    str++;
  }
  return num * sign;
}

void strlower(char* str) {
  for (uint8_t i = 0; str[i]; i++)
    if (str[i] >= 'A' && str[i] <= 'Z') str[i] += 32;
}

void stripQuotes(char* str) {
  int len = strlen(str);
  if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
    memmove(str, str+1, len-2);
    str[len-2] = 0;
  }
}

void parseCommand(char* line, char** argv, uint8_t* argc) {
  *argc = 0;
  char* token = strtok(line, " ");
  while (token != NULL && *argc < MAX_ARGS) {
    argv[(*argc)++] = token;
    token = strtok(NULL, " ");
  }
}

int8_t resolvePin(const char* name) {
  if ((name[0] == 'D' || name[0] == 'd') && name[1] >= '2' && name[1] <= '9')
    return name[1] - '0';
  if (name[0] >= '0' && name[0] <= '9') {
    int p = safeAtoi(name);
    if (p >= 2 && p <= 13) return p;
  }
  if ((name[0] == 'A' || name[0] == 'a') && name[1] >= '0' && name[1] <= '5')
    return 14 + (name[1] - '0');
  if (!strcmp(name, "LED") || !strcmp(name, "led")) return LED_BUILTIN;
  if (!strcmp(name, "TX") || !strcmp(name, "tx")) return 1;
  if (!strcmp(name, "RX") || !strcmp(name, "rx")) return 0;
  return -1;
}