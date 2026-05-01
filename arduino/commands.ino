void cmdPinMode(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: pinmode <pin> <mode>")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  strlower(argv[2]);
  if (!strcmp(argv[2], "output") || !strcmp(argv[2], "out")) {
    pinMode(pin, OUTPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> OUTPUT"));
  } else if (!strcmp(argv[2], "input_pullup") || !strcmp(argv[2], "pullup")) {
    pinMode(pin, INPUT_PULLUP);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT_PULLUP"));
  } else if (!strcmp(argv[2], "pwm")) {
    if (pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11) {
      pinMode(pin, OUTPUT);
      Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> PWM ready"));
    } else Serial.println(F("PWM only on pins 3,5,6,9,10,11"));
  } else {
    pinMode(pin, INPUT);
    Serial.print(F("Pin ")); Serial.print(pin); Serial.println(F(" -> INPUT"));
  }
}

void cmdDigitalWrite(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: write/gpio <pin> <HIGH|LOW|ON|OFF|TOGGLE>")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  strlower(argv[2]);
  int value;
  if (!strcmp(argv[2], "high") || !strcmp(argv[2], "1") || !strcmp(argv[2], "on"))
    value = HIGH;
  else if (!strcmp(argv[2], "low") || !strcmp(argv[2], "0") || !strcmp(argv[2], "off"))
    value = LOW;
  else if (!strcmp(argv[2], "toggle")) {
    pinMode(pin, OUTPUT);
    int current = digitalRead(pin);
    value = !current;
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
    Serial.print(F(" = ")); Serial.println(value ? F("HIGH") : F("LOW"));
  }
}

void cmdDigitalRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("\n  Digital Pin States"));
    for (int p = 2; p <= 13; p++) {
      pinMode(p, INPUT_PULLUP);
      delay(2);
      Serial.print(F("  ")); if (p<10) Serial.print(' ');
      Serial.print(p); Serial.print(F("  |  "));
      Serial.println(digitalRead(p) ? F("HIGH ") : F("LOW  "));
    }
    return;
  }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  pinMode(pin, INPUT_PULLUP);
  delay(5);
  Serial.print(F("Pin ")); Serial.print(pin);
  Serial.print(F(" = ")); Serial.println(digitalRead(pin) ? F("HIGH") : F("LOW"));
}

void cmdAnalogRead(char** argv, uint8_t argc) {
  if (argc < 2) {
    Serial.println(F("\n  Analog Inputs"));
    for (int a = 0; a <= 5; a++) {
      int raw = analogRead(a);
      Serial.print(F("  A")); Serial.print(a); Serial.print(F(" | "));
      Serial.print(raw); Serial.print(F(" | ")); Serial.print(raw * 5.0 / 1023.0, 2); Serial.println('V');
    }
    return;
  }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 14 || pin > 19) { Serial.println(F("Use A0-A5")); return; }
  int raw = analogRead(pin - 14);
  Serial.print(F("A")); Serial.print(pin - 14);
  Serial.print(F(" = ")); Serial.print(raw);
  Serial.print(F(" (")); Serial.print(raw * 5.0 / 1023.0, 2); Serial.println(F("V)"));
}

void cmdPWM(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: pwm <pin> <0-255>")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0 || (pin != 3 && pin != 5 && pin != 6 && pin != 9 && pin != 10 && pin != 11)) {
    Serial.println(F("PWM pins: 3,5,6,9,10,11"));
    return;
  }
  int val = constrain(safeAtoi(argv[2]), 0, 255);
  analogWrite(pin, val);
  Serial.print(F("PWM ")); Serial.print(pin); Serial.print(F(" = ")); Serial.print(val);
  Serial.print(F(" (")); Serial.print(map(val,0,255,0,100)); Serial.println(F("%)"));
}

void cmdTone(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: tone <pin> <freq> [ms]")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) { Serial.println(F("Invalid pin")); return; }
  int freq = safeAtoi(argv[2]);
  if (argc >= 4) tone(pin, freq, safeAtoi(argv[3]));
  else tone(pin, freq);
  Serial.println(F("Done"));
}

void cmdNoTone(char** argv, uint8_t argc) {
  if (argc < 2) {
    for (int p = 2; p <= 13; p++) noTone(p);
    Serial.println(F("All tones stopped"));
  } else {
    int8_t pin = resolvePin(argv[1]);
    if (pin >= 0) noTone(pin);
  }
}

void cmdLEDDisco(char** argv, uint8_t argc) {
  int cycles = (argc >= 2) ? safeAtoi(argv[1]) : 3;
  if (cycles > 20) cycles = 20;
  int speed = (argc >= 3) ? safeAtoi(argv[2]) : 30;
  if (speed < 10) speed = 10;
  Serial.println(F("\n  DISCO"));
  for (int c = 0; c < cycles; c++) {
    for (int p = 2; p <= 13; p++) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); delay(speed); digitalWrite(p, LOW); }
    for (int p = 12; p >= 3; p--) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); delay(speed); digitalWrite(p, LOW); }
    for (int i = 0; i < 8; i++) {
      for (int p = 2; p <= 9; p++) { pinMode(p, OUTPUT); digitalWrite(p, (i>>(p-2))&1); }
      delay(speed*3);
    }
  }
  for (int p = 2; p <= 13; p++) pinMode(p, INPUT_PULLUP);
  Serial.println(F("  Done"));
}

void cmdSensor() {
  Serial.println(F("\n  Sensor Monitor"));
  for (int a = 0; a <= 5; a++) {
    long sum = 0;
    for (int i = 0; i < 5; i++) sum += analogRead(a);
    int avg = sum / 5;
    int barLen = map(avg, 0, 1023, 0, 20);
    Serial.print(F("  A")); Serial.print(a); Serial.print(F(" ["));
    for (int b = 0; b < 20; b++) Serial.print(b < barLen ? F("#") : F("-"));
    Serial.print(F("] ")); Serial.print(avg); Serial.print(F(" (")); Serial.print(avg * 5.0 / 1023.0, 2); Serial.println(F("V)"));
  }
}

void cmdScope(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: scope <A0-A5> [samples]")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 14) { Serial.println(F("Use A0-A5")); return; }
  int samples = (argc >= 3) ? safeAtoi(argv[2]) : 50;
  samples = constrain(samples, 10, 100);
  int values[100];
  for (int i = 0; i < samples; i++) { values[i] = analogRead(pin - 14); delay(5); }
  int vmin = 1023, vmax = 0;
  for (int i = 0; i < samples; i++) {
    if (values[i] < vmin) vmin = values[i];
    if (values[i] > vmax) vmax = values[i];
  }
  Serial.print(F("\n  Scope A")); Serial.print(pin-14); Serial.println();
  for (int row = 15; row >= 0; row--) {
    Serial.print(F("  "));
    for (int i = 0; i < samples; i++) {
      int mapped = map(values[i], vmin, vmax, 0, 15);
      Serial.print(mapped == row ? F("#") : (mapped > row ? F("|") : F(" ")));
    }
    Serial.println();
  }
}

void cmdMorse(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: morse <pin> <msg>")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) return;
  pinMode(pin, OUTPUT);
  const char* morseTable[] = {
    ".-","-...","-.-.","-..",".","..-.","--.","....","..",".---",
    "-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-",
    "..-","...-",".--","-..-","-.--","--.."
  };
  for (int i = 2; i < argc; i++) {
    for (char* c = argv[i]; *c; c++) {
      char ch = *c;
      if (ch >= 'a' && ch <= 'z') ch -= 32;
      if (ch >= 'A' && ch <= 'Z') {
        const char* code = morseTable[ch - 'A'];
        for (const char* m = code; *m; m++) {
          digitalWrite(pin, HIGH); delay(*m == '.' ? 100 : 300);
          digitalWrite(pin, LOW); delay(100);
        }
        delay(300);
      } else if (ch == ' ') delay(700);
    }
  }
  Serial.println(F(" Done"));
}

void cmdMONITOR(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: monitor <pin> <ms> [sec]")); return; }
  int8_t pin = resolvePin(argv[1]);
  if (pin < 0) return;
  int interval = safeAtoi(argv[2]);
  if (interval < 100) interval = 100;
  int duration = (argc >= 4) ? safeAtoi(argv[3]) : 10;
  bool isAnalog = (pin >= 14);
  unsigned long stop = millis() + duration * 1000UL;
  while (millis() < stop) {
    int val = isAnalog ? analogRead(pin - 14) : (pinMode(pin, INPUT_PULLUP), digitalRead(pin));
    Serial.print(F("[t+")); Serial.print((millis() - (stop - duration*1000UL))/1000);
    Serial.print(F("s] "));
    if (isAnalog) Serial.print(val); else Serial.print(val ? F("HIGH") : F("LOW"));
    Serial.println();
    delay(interval);
  }
}

void cmdEVAL(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: eval \"cmd1; cmd2; ...\"")); return; }
  char code[CONTENT_LEN] = "";
  for (int i = 1; i < argc; i++) {
    if (i > 1) strncat(code, " ", CONTENT_LEN - strlen(code) - 1);
    strncat(code, argv[i], CONTENT_LEN - strlen(code) - 1);
  }
  stripQuotes(code);
  Serial.println(F("\n  >>> Eval <<<"));
  char* cmd = strtok(code, ";");
  int n = 0;
  while (cmd) {
    while (*cmd == ' ') cmd++;
    stripQuotes(cmd);
    n++; Serial.print(F("  [")); Serial.print(n); Serial.print(F("] $ ")); Serial.println(cmd);
    executeCommand(cmd);
    cmd = strtok(NULL, ";");
    delay(50);
  }
  Serial.println(F("  >>> Done <<<\n"));
}

void cmdRUN(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: run <script>")); return; }
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0 || IS_DIR(fs[idx])) { Serial.println(F("Not a script")); return; }
  Serial.print(F("\n  >>> Running ")); Serial.print(argv[1]); Serial.println(F(" <<<"));
  char script[CONTENT_LEN];
  strncpy(script, fs[idx].content, CONTENT_LEN - 1);
  char* line = strtok(script, "\n");
  int n = 0;
  while (line) {
    n++; Serial.print(F("  [")); Serial.print(n); Serial.print(F("] $ ")); Serial.println(line);
    executeCommand(line);
    line = strtok(NULL, "\n");
    delay(50);
  }
  Serial.println(F("  >>> Done <<<\n"));
}

void cmdDELAY(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: delay <ms>")); return; }
  int ms = safeAtoi(argv[1]);
  if (ms > 30000) ms = 30000;
  delay(ms);
}

void cmdFOR(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: for <count> \"command\"")); return; }
  int count = safeAtoi(argv[1]);
  if (count > 100) count = 100;
  char cmd[CONTENT_LEN] = "";
  for (int i = 2; i < argc; i++) {
    if (i > 2) strncat(cmd, " ", CONTENT_LEN - strlen(cmd) - 1);
    strncat(cmd, argv[i], CONTENT_LEN - strlen(cmd) - 1);
  }
  stripQuotes(cmd);
  Serial.print(F("Loop ")); Serial.print(count); Serial.println(F(" times:"));
  for (int i = 0; i < count; i++) {
    Serial.print(F("\r  [")); Serial.print(i+1); Serial.print('/'); Serial.print(count); Serial.print(']');
    executeCommand(cmd);
    delay(10);
  }
  Serial.println();
}

void cmdHELP() {
  Serial.println(F("\n  ArduinOS v1.0.3 Commands"));
  Serial.println(F("  Hardware: pinmode, write, read, aread, pwm, tone, gpio, disco, sensor, scope, morse, monitor"));
  Serial.println(F("  Files:    ls, cd, pwd, mkdir, touch, cat, writefile, rm"));
  Serial.println(F("  Code:     eval, run, for, delay"));
  Serial.println(F("  System:   help, sysinfo, dmesg, uptime, free, whoami, uname, clear, reboot\n"));
}

void cmdSYSINFO() {
  unsigned long sec = (millis() - bootTime) / 1000;
  uint8_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
  Serial.println(F("\n  System Information"));
  Serial.print(F("  OS:    ArduinOS v1.0.3\n  Host:  Arduino UNO\n"));
  Serial.print(F("  CPU:   ATmega328P 16MHz\n  RAM:   "));
  Serial.print(freeRAM()); Serial.println(F("/2048 bytes"));
  Serial.print(F("  Uptime: "));
  if (h < 10) Serial.print('0'); Serial.print(h); Serial.print(':');
  if (m < 10) Serial.print('0'); Serial.print(m); Serial.print(':');
  if (s < 10) Serial.print('0'); Serial.println(s);
}