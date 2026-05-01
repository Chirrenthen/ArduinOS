void cmdDMESG() {
  Serial.println(F("\n  Kernel Log"));
  for (int i = 0; i < dmesgCount; i++) {
    uint8_t idx = (dmesgIndex - 1 - i + DMESG_LINES) % DMESG_LINES;
    if (dmesg[idx].message[0]) {
      Serial.print(F("  [")); Serial.print(dmesg[idx].timestamp); Serial.print(F("s] "));
      Serial.println(dmesg[idx].message);
    }
  }
}

void executeCommand(char* line) {
  while (*line == ' ') line++;
  if (!*line || *line == '#') return;
  char* argv[MAX_ARGS];
  uint8_t argc;
  parseCommand(line, argv, &argc);
  if (argc == 0) return;
  strlower(argv[0]);

  if (!strcmp(argv[0], "pinmode")) cmdPinMode(argv, argc);
  else if (!strcmp(argv[0], "write") || !strcmp(argv[0], "digitalwrite")) cmdDigitalWrite(argv, argc);
  else if (!strcmp(argv[0], "read") || !strcmp(argv[0], "digitalread")) cmdDigitalRead(argv, argc);
  else if (!strcmp(argv[0], "aread") || !strcmp(argv[0], "analogread")) cmdAnalogRead(argv, argc);
  else if (!strcmp(argv[0], "pwm") || !strcmp(argv[0], "analogwrite")) cmdPWM(argv, argc);
  else if (!strcmp(argv[0], "tone")) cmdTone(argv, argc);
  else if (!strcmp(argv[0], "notone")) cmdNoTone(argv, argc);
  else if (!strcmp(argv[0], "gpio")) {
    if (argc >= 2 && !strcmp(argv[1], "vixa")) cmdLEDDisco(argv, argc);
    else if (argc >= 3) cmdDigitalWrite(argv, argc);
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
  else if (!strcmp(argv[0], "writefile")) cmdWRITE(argv, argc);
  else if (!strcmp(argv[0], "echo")) {
    if (argc >= 3 && !strcmp(argv[argc-2], ">")) cmdWRITE(argv, argc);
    else { for (int i = 1; i < argc; i++) { Serial.print(argv[i]); Serial.print(' '); } Serial.println(); }
  }
  else if (!strcmp(argv[0], "rm") || !strcmp(argv[0], "del")) cmdRM(argv, argc);
  else if (!strcmp(argv[0], "help") || !strcmp(argv[0], "?")) cmdHELP();
  else if (!strcmp(argv[0], "sysinfo") || !strcmp(argv[0], "neofetch")) cmdSYSINFO();
  else if (!strcmp(argv[0], "dmesg") || !strcmp(argv[0], "log")) cmdDMESG();
  else if (!strcmp(argv[0], "uptime")) {
    unsigned long s = (millis() - bootTime) / 1000;
    uint8_t h = s/3600, m = (s%3600)/60, sec = s%60;
    if (h<10) Serial.print('0'); Serial.print(h); Serial.print(':');
    if (m<10) Serial.print('0'); Serial.print(m); Serial.print(':');
    if (sec<10) Serial.print('0'); Serial.println(sec);
  }
  else if (!strcmp(argv[0], "free") || !strcmp(argv[0], "df")) {
    Serial.print(F("RAM free: ")); Serial.print(freeRAM()); Serial.println(F(" bytes"));
    uint8_t used = 0; for (int i=0; i<MAX_FILES; i++) if (IS_ACTIVE(fs[i])) used++;
    Serial.print(F("Files: ")); Serial.print(used); Serial.print('/'); Serial.println(MAX_FILES);
  }
  else if (!strcmp(argv[0], "whoami")) Serial.println(F("root"));
  else if (!strcmp(argv[0], "uname")) Serial.println(F("ArduinOS v1.0.3"));
  else if (!strcmp(argv[0], "clear") || !strcmp(argv[0], "cls")) {
    for (int i=0;i<30;i++) Serial.println();
  }
  else if (!strcmp(argv[0], "reboot")) {
    Serial.println(F("Rebooting..."));
    delay(500);
    softReset();
  }
  else { Serial.print(F("Unknown: ")); Serial.println(argv[0]); }
}