#include <stdio.h>
#include <string.h>
#include "aes.c"

void handleHelp(char*);
void handlePing(char*);
void handleTextMsg(char*);
void handleAESMsg(char *);
void handleSettings(char*);
void handleFreq(char*);
void handleBW(char*);
void handleSF(char*);
void handleCR(char*);
void handleTX(char*);
void handleAP(char*);
void handleAES(char*);
void handlePassword(char*);

vector<string> userStrings;
int cmdCount = 0;

struct myCommand {
  void (*ptr)(char *); // Function pointer
  char name[12];
  char help[48];
};

myCommand cmds[] = {
  {handleHelp, "help", "Shows this help."},
  {handleSettings, "lora", "Gets the current LoRa settings."},
  {handleFreq, "fq", "Gets/sets the working frequency."},
  {handleBW, "bw", "Gets/sets the working bandwidth."},
  {handleSF, "sf", "Gets/sets the Mspreading factor."},
  {handleCR, "cr", "Gets/sets the working coding rate."},
  {handleTX, "tx", "Gets/sets the Transmission Power."},
  {handleAP, "ap", "Gets/sets the auto-ping rate."},
  {handleAES, "aes", "Gets/sets the AES mode."},
  {handlePassword, "pwd", "Gets/sets the AES password."},
  {handlePing, "p", "Sends a ping packet."},
};

void handleHelp(char *param) {
  Serial.printf("Available commands: %d\n", cmdCount);
  for (int i = 0; i < cmdCount; i++) {
    sprintf(msg, " . %s: %s", cmds[i].name, cmds[i].help);
    Serial.println(msg);
  }
}

void evalCmd(char *str, string fullString) {
  uint8_t ix, iy = strlen(str);
  for (ix = 0; ix < iy; ix++) {
    char c = str[ix];
    // lowercase the keyword
    if (c >= 'A' && c <= 'Z') str[ix] = c + 32;
  }
  Serial.print("Evaluating: `");
  Serial.print(fullString.c_str());
  Serial.println("`");
  for (int i = 0; i < cmdCount; i++) {
    if (strcmp(str, cmds[i].name) == 0) {
      cmds[i].ptr((char*)fullString.c_str());
      return;
    }
  }
  handleHelp("");
}

void handleCommands(string str1) {
  char kwd[32];
  int i = sscanf((char*)str1.c_str(), "/%s", kwd);
  if (i > 0) evalCmd(kwd, str1);
  else handleTextMsg((char*)str1.c_str());
}

void handleSettings(char *param) {
  Serial.println("Current settings:");
  handleFreq("/fq");
  handleBW("/bw");
  handleSF("/sf");
  handleCR("/cr");
  handleTX("/tx");
  handleAP("/ap");
}

void handlePing(char *param) {
  sendMode();
  delay(100);
  memset(msg, 0, 128);
  strcpy(msg, "This is a not so short PING from ");
  strcpy(msg + 33, myName);
  Serial.println(msg);
  if (needAES) handleAESMsg(msg);
  else handleTextMsg(msg);
  listenMode();
  lastPing = millis();
  // whether a manual ping or automatic
}

void handleAESMsg(char *what) {
  fillRandom(myIV, 16);
  Serial.println("IV:");
  hexDump(myIV, 16);
  int16_t ln = strlen(what);
  int16_t olen = encryptCBC((uint8_t*)what, ln, myPWD, myIV);
  sendMode();
  delay(100);
  Serial.println("Sending:");
  hexDump(encBuf, olen);
  uint32_t t0 = millis();
  LoRa.beginPacket();
  LoRa.write(myIV, 16);
  LoRa.write(encBuf, olen);
  LoRa.endPacket();
  uint32_t t1 = millis();
  Serial.print("Done! Time: ");
  Serial.print(t1 - t0);
  Serial.println(" ms.");
  listenMode();
}

void handleTextMsg(char *what) {
  sendMode();
  delay(100);
  Serial.print("Sending `");
  Serial.print(what);
  uint32_t t0 = millis();
  LoRa.beginPacket();
  LoRa.write((uint8_t*)what, strlen(what));
  LoRa.endPacket();
  uint32_t t1 = millis();
  Serial.print("` done!\nTime: ");
  Serial.print(t1 - t0);
  Serial.println(" ms.");
  listenMode();
}

void handleFreq(char *param) {
  if (strcmp("/fq", param) == 0) {
    // no parameters
    Serial.print(" - Frequency: ");
    Serial.print(myFreq / 1e6);
    Serial.println(" MHz");
  } else {
    // fq xxx.xxx set frequency
    // float value = sscanf(param, "%*s %f", &value);
    float value = atof(param + 3);
    if (value < 150.0 || value > 960.0) {
      // freq range 150MHz to 960MHz
      // Your chip might not support all...
      Serial.print("Invalid frequency value: ");
      Serial.println(value);
      return;
    }
    myFreq = (uint32_t)(value * 1e6);
    Serial.print("New freq: "); Serial.println((float)(myFreq / 1e6), 3);
    LoRa.sleep();
    LoRa.setFrequency(myFreq);
    listenMode();
  }
#if defined(TRUST_BUT_VERIFY)
  checkFreq();
#endif
}

void handleBW(char* param) {
  if (strcmp("/bw", param) == 0) {
    // no parameters
    Serial.print(" - BW: "); Serial.print(bw);
    Serial.print(", ie "); Serial.print(myBWs[bw]); Serial.println("KHz");
  } else {
    int value = atoi(param + 3);
    // bw xxxx set BW
    if (value > 9) {
      Serial.print("Invalid BW value: ");
      Serial.println(value);
      return;
    }
    bw = value;
    LoRa.sleep();
    LoRa.setSignalBandwidth(myBWs[bw] * 1e3);
    listenMode();
    Serial.print("BW set to "); Serial.print(bw);
    Serial.print(", ie "); Serial.print(myBWs[bw]); Serial.println(" KHz");
  }
#if defined(TRUST_BUT_VERIFY)
  checkBW();
#endif
}

void handleSF(char* param) {
  if (strcmp("/sf", param) == 0) {
    // no parameters
    Serial.print(" - SF: "); Serial.println(sf);
    return;
  } else {
    int value = atoi(param + 3);
    // bw xxxx set BW
    if (value < 6 || value > 12) {
      Serial.print("Invalid SF value: ");
      Serial.println(value);
      return;
    }
    sf = value;
    LoRa.sleep();
    LoRa.setSpreadingFactor(sf);
    listenMode();
    Serial.print("SF set to "); Serial.println(sf);
  }
#if defined(TRUST_BUT_VERIFY)
  checkSF();
#endif
}

void handleCR(char* param) {
  if (strcmp("/cr", param) == 0) {
    // no parameters
    Serial.print(" - CR 4/"); Serial.println(cr);
    return;
  } else {
    int value = atoi(param + 3);
    // bw xxxx set BW
    if (value < 5 || value > 8) {
      Serial.print("Invalid CR value: ");
      Serial.println(value);
      return;
    }
    cr = value;
    LoRa.sleep();
    LoRa.setCodingRate4(cr);
    listenMode();
    Serial.print("CR set to 4/"); Serial.println(cr);
  }
#if defined(TRUST_BUT_VERIFY)
  checkCR();
#endif
}

void handleTX(char* param) {
  if (strcmp("/tx", param) == 0) {
    // no parameters
    Serial.print(" - Tx power: "); Serial.println(txPower);
    return;
  }
  int value = atoi(param + 3);
  // bw xxxx set BW
  if (value < 5 || value > 8) {
    Serial.print("Invalid Tx value: ");
    Serial.println(value);
    return;
  }
  txPower = value;
  LoRa.sleep();
  LoRa.setTxPower(txPower);
  listenMode();
  Serial.print("Tx power set to: "); Serial.println(txPower);
  return;
}

void handleAP(char* param) {
  if (strcmp("/ap", param) == 0) {
    // no parameters
    Serial.print(" - AutoPING: ");
    if (apFreq == 0) Serial.println("OFF.");
    else {
      Serial.print("ON, every ");
      Serial.print(apFreq / 1000);
      Serial.println(" secs.");
    }
    return;
  }
  uint32_t value = atoi(param + 3);
  if (value > 0 && value < 10) {
    Serial.println("Unless you want to turn if off (/ap 0), please pass a value of 10 seconds or more.");
    return;
  }
  apFreq = value * 1e3;
  Serial.print("AutoPING set to: ");
  if (apFreq == 0) Serial.println("OFF.");
  else {
    Serial.print("ON, every ");
    Serial.print(apFreq / 1000);
    Serial.println(" secs.");
    lastPing = millis();
  }
}

int16_t encryptCBC(uint8_t* myBuf, uint8_t olen, uint8_t* pKey, uint8_t* Iv) {
  uint8_t rounds = olen / 16;
  if (rounds == 0) rounds = 1;
  else if (olen - (rounds * 16) != 0) rounds += 1;
  uint8_t length = rounds * 16;
  memset(encBuf, (length - olen), length);
  memcpy(encBuf, myBuf, olen);
  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, pKey, Iv);
  AES_CBC_encrypt_buffer(&ctx, encBuf, length);
  return length;
}

int16_t decryptCBC(uint8_t* myBuf, uint8_t olen, uint8_t* pKey, uint8_t* Iv) {
  memcpy(encBuf, myBuf, olen);
  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, pKey, Iv);
  AES_CBC_decrypt_buffer(&ctx, encBuf, olen);
  return olen;
}

void handlePassword(char* param) {
  char pwd[33];
  memset(pwd, 0, 33);
  int i = sscanf(param, "%*s %s", pwd);
  if (i == -1) {
    // no parameters
    Serial.println("pwd: yeah right!");
    return;
  } else {
    // either 16 chars or 32 hex chars
    if (strlen(pwd) == 16) {
      memcpy(myPWD, pwd, 16);
      Serial.println("Password set from string.");
      return;
    } else if (strlen(pwd) == 32) {
      hex2array(pwd, myPWD, 32);
      Serial.println("Password set from hex string.");
      Serial.println(msg);
      return;
    }
    Serial.println("AES: wrong pwd size! It should be 16 bytes, or a 32-byte hex string!");
    return;
  }
}

void handleAES(char* param) {
  char sw[33];
  memset(sw, 0, 33);
  int i = sscanf(param, "%*s %s", sw);
  if (i == -1) {
    // no parameters
    Serial.print("AES mode: ");
    if (needAES) Serial.println("ON");
    else Serial.println("OFF");
    return;
  } else {
    for (i = 0; i < strlen(sw); i++) {
      // UPPERCASE everything
      if (sw[i] >= 'a' && sw[i] <= 'z') sw[i] -= 32;
    }
    if (strcmp("ON", sw) == 0) {
      needAES = true;
      Serial.println("Turning AES ON");
      checkPWD();
    } else if (strcmp("OFF", sw) == 0) {
      needAES = false;
      Serial.println("Turning AES OFF");
    } else {
      Serial.print("Unknown parameter: "); Serial.println(sw);
    }
  }
}
