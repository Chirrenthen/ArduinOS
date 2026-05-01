int8_t findFile(const char* name, const char* path) {
  for (uint8_t i = 0; i < MAX_FILES; i++)
    if (IS_ACTIVE(fs[i]) && !strcmp(fs[i].name, name) && !strcmp(fs[i].parentDir, path))
      return i;
  return -1;
}

int8_t findFreeSlot() {
  for (uint8_t i = 0; i < MAX_FILES; i++)
    if (!IS_ACTIVE(fs[i])) return i;
  return -1;
}

void initFileSystem() {
  memset(fs, 0, sizeof(fs));
  const char* dirs[] = {"home", "dev", "tmp", "etc"};
  for (uint8_t d = 0; d < 4; d++) {
    int8_t slot = findFreeSlot();
    if (slot >= 0) {
      strncpy(fs[slot].name, dirs[d], NAME_LEN - 1);
      strncpy(fs[slot].parentDir, "/", PATH_LEN - 1);
      fs[slot].flags = 3;
      fs[slot].created = millis();
    }
  }
  klog("Filesystem initialized");
}

void cmdLS() {
  Serial.println(F("\n  Name         Type  Size  Created"));
  uint8_t cnt = 0;
  for (uint8_t i = 0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && !strcmp(fs[i].parentDir, currentPath)) {
      Serial.print(F("  ")); Serial.print(fs[i].name);
      for (uint8_t s = strlen(fs[i].name); s < 13; s++) Serial.print(' ');
      Serial.print(IS_DIR(fs[i]) ? F("DIR  ") : F("FILE "));
      if (!IS_DIR(fs[i])) {
        Serial.print(strlen(fs[i].content));
        Serial.print(F("b  "));
      } else {
        Serial.print(F(" -   "));
      }
      unsigned long age = (millis() - fs[i].created) / 1000;
      Serial.print(age);
      Serial.println('s');
      cnt++;
    }
  }
  if (cnt == 0) Serial.println(F("  (empty)"));
  Serial.print(F("  ")); Serial.print(cnt); Serial.println(F(" items\n"));
}

void cmdMKDIR(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: mkdir <name>")); return; }
  int8_t slot = findFreeSlot();
  if (slot < 0) { Serial.println(F("No space")); return; }
  strncpy(fs[slot].name, argv[1], NAME_LEN - 1);
  strncpy(fs[slot].parentDir, currentPath, PATH_LEN - 1);
  fs[slot].flags = 3;
  fs[slot].created = millis();
  Serial.print(F("Dir ")); Serial.print(argv[1]); Serial.println(F(" created"));
}

void cmdTOUCH(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: touch <file>")); return; }
  int8_t idx = findFile(argv[1], currentPath);
  if (idx >= 0) { fs[idx].created = millis(); Serial.println(F("Updated")); return; }
  idx = findFreeSlot();
  if (idx < 0) { Serial.println(F("No space")); return; }
  strncpy(fs[idx].name, argv[1], NAME_LEN - 1);
  strncpy(fs[idx].parentDir, currentPath, PATH_LEN - 1);
  fs[idx].flags = 1;
  fs[idx].created = millis();
  fs[idx].content[0] = 0;
  Serial.print(F("File ")); Serial.print(argv[1]); Serial.println(F(" created"));
}

void cmdCAT(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: cat <file>")); return; }
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) { Serial.println(F("Not found")); return; }
  if (IS_DIR(fs[idx])) { Serial.println(F("Is a directory")); return; }
  if (strlen(fs[idx].content) > 0) {
    for (char* c = fs[idx].content; *c; c++) {
      if (*c == '\\' && *(c+1) == 'n') { Serial.println(); c++; }
      else Serial.print(*c);
    }
    Serial.println();
  } else Serial.println(F("(empty)"));
}

void cmdWRITE(char** argv, uint8_t argc) {
  if (argc < 3) { Serial.println(F("Usage: writefile <file> <content>")); return; }
  char content[CONTENT_LEN] = "";
  for (int i = 2; i < argc; i++) {
    if (i > 2) strncat(content, " ", CONTENT_LEN - strlen(content) - 1);
    strncat(content, argv[i], CONTENT_LEN - strlen(content) - 1);
  }
  stripQuotes(content);
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) {
    idx = findFreeSlot();
    if (idx < 0) { Serial.println(F("No space")); return; }
    strncpy(fs[idx].name, argv[1], NAME_LEN - 1);
    strncpy(fs[idx].parentDir, currentPath, PATH_LEN - 1);
    fs[idx].flags = 1;
    fs[idx].created = millis();
  }
  char finalContent[CONTENT_LEN];
  char* dst = finalContent;
  for (char* src = content; *src && (dst - finalContent) < CONTENT_LEN - 1; src++) {
    if (*src == '\\' && *(src+1) == 'n') {
      *dst++ = '\n';
      src++;
    } else {
      *dst++ = *src;
    }
  }
  *dst = 0;
  strncpy(fs[idx].content, finalContent, CONTENT_LEN - 1);
  fs[idx].content[CONTENT_LEN - 1] = 0;
  Serial.print(F("Written to ")); Serial.println(argv[1]);
}

void cmdRM(char** argv, uint8_t argc) {
  if (argc < 2) { Serial.println(F("Usage: rm <name>")); return; }
  int8_t idx = findFile(argv[1], currentPath);
  if (idx < 0) { Serial.println(F("Not found")); return; }
  if (IS_DIR(fs[idx])) {
    char dirPath[PATH_LEN];
    snprintf(dirPath, PATH_LEN, "%s%s/", currentPath, argv[1]);
    for (int i = 0; i < MAX_FILES; i++)
      if (IS_ACTIVE(fs[i]) && !strncmp(fs[i].parentDir, dirPath, strlen(dirPath)))
        fs[i].flags = 0;
  }
  fs[idx].flags = 0;
  Serial.println(F("Removed"));
}

void cmdCD(char** argv, uint8_t argc) {
  if (argc < 2 || !strcmp(argv[1], "/")) {
    strncpy(currentPath, "/", PATH_LEN - 1);
    return;
  }
  if (!strcmp(argv[1], "..")) {
    if (!strcmp(currentPath, "/")) return;
    int len = strlen(currentPath);
    if (len <= 1) { strncpy(currentPath, "/", PATH_LEN - 1); return; }
    currentPath[len - 1] = 0;
    char* lastSlash = strrchr(currentPath, '/');
    if (lastSlash) *(lastSlash + 1) = 0;
    else strncpy(currentPath, "/", PATH_LEN - 1);
    return;
  }
  for (int i = 0; i < MAX_FILES; i++) {
    if (IS_ACTIVE(fs[i]) && IS_DIR(fs[i]) && !strcmp(fs[i].name, argv[1]) &&
        !strcmp(fs[i].parentDir, currentPath)) {
      if (strlen(currentPath) + strlen(fs[i].name) + 1 >= PATH_LEN) {
        Serial.println(F("Path too long"));
        return;
      }
      strcat(currentPath, fs[i].name);
      strcat(currentPath, "/");
      return;
    }
  }
  Serial.print(F("cd: ")); Serial.print(argv[1]); Serial.println(F(" not found"));
}