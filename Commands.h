#include <stdio.h>
#include <string.h>

void handleHelp(char*);
void handlePing(char*);
void handleTextMsg(char*);
void handleFreq(char*);
void handleBW(char*);
void handleSF(char*);
void handleCR(char*);

vector<string> userStrings;
int cmdCount = 0;
char msg[128]; // general-use buffer
uint32_t myFreq = 470e6;
uint16_t sf = 12;
uint16_t bw = 7;
uint16_t cr = 5;
uint16_t preamble = 8;
uint16_t txPower = 20;
float myBWs[10] = {7.8, 10.4, 15.63, 20.83, 31.25, 41.67, 62.5, 125, 250, 500};

struct myCommand {
  void (*ptr)(char *); // Function pointer
  char name[12];
  char help[48];
};

myCommand cmds[] = {
  {handleHelp, "help", "Shows this help."},
  {handleFreq, "fq", "Gets/sets the working frequency."},
  {handleBW, "bw", "Gets/sets the working bandwidth."},
  {handleSF, "sf", "Gets/sets the Mspreading factor."},
  {handleCR, "cr", "Gets/sets the working coding rate."},
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
    // Serial.print(" - ");
    // Serial.print(cmds[i].name);
    // Serial.print(" vs ");
    // Serial.println(str);
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

void handlePing(char *param) {
  sendMode();
  delay(100);
  strcpy(msg, "This is a not so short PING!");
  handleTextMsg(msg);
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
    Serial.print("Frequency: ");
    Serial.print(myFreq / 1e6);
    Serial.println(" MHz");
    return;
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
    //    unsigned char f0 = LoRa.readRegister(RegFrfLsb);
    //    unsigned char f1 = LoRa.readRegister(RegFrfMid);
    //    unsigned char f2 = LoRa.readRegister(RegFrfMsb);
    //    unsigned long lfreq = f2; lfreq = (lfreq << 8) + f1; lfreq = (lfreq << 8) + f0;
    //    float newFreq = (lfreq * 61.035) / 1000000;
    //    Serial.print("Check: "); Serial.println(newFreq);
    return;
  }
}

void handleBW(char* param) {
  if (strcmp("/bw", param) == 0) {
    // no parameters
    Serial.print("BW: "); Serial.print(bw);
    Serial.print(", ie "); Serial.print(myBWs[bw]); Serial.println("KHz");
    return;
  }
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
#if defined(TRUST_BUT_VERIFY)
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_1);
  uint8_t x = vv >> 4;
  Serial.print(" . BW: " + String(x) + " [" + String((float)myBWs[x], 3) + " KHz]");
  Serial.print(F(" ---> "));
  if (bw == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
#endif
  return;
}

void handleSF(char* param) {
  if (strcmp("/sf", param) == 0) {
    // no parameters
    Serial.print("SF: "); Serial.println(sf);
    return;
  }
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
#if defined(TRUST_BUT_VERIFY)
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_2);
  uint8_t x = vv >> 4;
  Serial.print(" . SF: " + String(x));
  Serial.print(F(" ---> "));
  if (sf == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
#endif
  return;
}

void handleCR(char* param) {
  if (strcmp("/cr", param) == 0) {
    // no parameters
    Serial.print("CR 4/"); Serial.println(cr);
    return;
  }
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
#if defined(TRUST_BUT_VERIFY)
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_1);
  uint8_t x = ((vv & 0b00001110) >> 1) + 4;
  Serial.print(" . CR: 4/" + String(x));
  Serial.print(F(" ---> "));
  if (cr == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
#endif
  return;
}

void handleTX(char* param) {
  if (strcmp("/tx", param) == 0) {
    // no parameters
    Serial.print("Tx power: "); Serial.println(txPower);
    return;
  }
  int value = atoi(param + 3);
  // bw xxxx set BW
  if (value < 5 || value > 8) {
    Serial.print("Invalid CR value: ");
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
