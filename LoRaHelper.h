int16_t encryptCBC(uint8_t* myBuf, uint8_t olen, uint8_t* pKey, uint8_t* Iv);
int16_t decryptCBC(uint8_t* myBuf, uint8_t olen, uint8_t* pKey, uint8_t* Iv);
void fillRandom(uint8_t* x, size_t len);
void hexDump(uint8_t* buf, uint16_t len);

#define RegFrfMsb 0x06
#define RegFrfMid 0x07
#define RegFrfLsb 0x08
#define REG_LNA 0x0c
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_MODEM_CONFIG_3 0x26

// Uncomment the next line if you want the code
// to check settings after changing them
#define TRUST_BUT_VERIFY 1

uint32_t myFreq = 470e6, apFreq = 0, lastPing = 0;
uint16_t sf = 12;
uint16_t bw = 7;
uint16_t cr = 5;
uint16_t preamble = 8;
uint16_t txPower = 20;
float myBWs[10] = {7.8, 10.4, 15.63, 20.83, 31.25, 41.67, 62.5, 125, 250, 500};
bool needAES = false;
bool needJSON = false;
uint8_t myPWD[16] = {0};
uint8_t myIV[16] = {0};
char myName[32] = {0};
uint8_t encBuf[256];
char msg[128]; // general-use buffer

void listenMode() {
  pinMode(RADIO_RF_CRX_RX, OUTPUT);
  digitalWrite(RADIO_RF_CRX_RX, HIGH); // set LoRa to receive
  pinMode(RADIO_RF_CTX_PA, OUTPUT);
  digitalWrite(RADIO_RF_CTX_PA, LOW);
  LoRa.writeRegister(REG_LNA, 0x23); // TURN ON LNA FOR RECEIVE
  LoRa.receive();
}

void sendMode() {
  pinMode(RADIO_RF_CRX_RX, OUTPUT);
  digitalWrite(RADIO_RF_CRX_RX, LOW); // set LoRa to send
  pinMode(RADIO_RF_CTX_PA, OUTPUT);
  digitalWrite(RADIO_RF_CTX_PA, HIGH); // control LoRa by PA_BOOST
  LoRa.writeRegister(REG_LNA, 00); // TURN OFF LNA FOR TRANSMIT
}

void onReceive(int packetSize) {
  // received a packet
  if (!needAES) {
    Serial.print("Received packet '");
    // read packet
    for (int i = 0; i < packetSize; i++) {
      Serial.print((char)LoRa.read());
    }
    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", and SNR ");
    Serial.println(LoRa.packetSnr());
  } else {
    for (int i = 0; i < packetSize; i++) {
      encBuf[i] = (uint8_t)LoRa.read();
    }
    Serial.print("Received packet, size: "); Serial.print(packetSize);
    Serial.print(" with RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", and SNR ");
    Serial.println(LoRa.packetSnr());

    hexDump(encBuf, packetSize);
    if (packetSize < 32) {
      Serial.println("Size is too small for a properly formatted packet!");
      return;
    }
    memcpy(myIV, encBuf, 16);
    uint16_t olen = packetSize - 16;
    memcpy((uint8_t*)msg, encBuf + 16, olen);
    decryptCBC((uint8_t*)msg, olen, myPWD, myIV);
    uint8_t x = encBuf[olen - 1];
    encBuf[olen - x] = 0;
    Serial.println((char*)encBuf);
  }
  LoRa.receive();
}

void checkFreq() {
  unsigned char f0 = LoRa.readRegister(RegFrfLsb);
  unsigned char f1 = LoRa.readRegister(RegFrfMid);
  unsigned char f2 = LoRa.readRegister(RegFrfMsb);
  unsigned long lfreq = f2; lfreq = (lfreq << 8) + f1; lfreq = (lfreq << 8) + f0;
  float newFreq = (lfreq * 61.035) / 1e6;
  Serial.print(" - Frequency: " + String((float)newFreq, 3) + " MHz");
  Serial.print(F(" ---> "));
  if ((myFreq / 1e6) - newFreq < 1.0) {
    Serial.println(F("All good..."));
    Serial.println("   Frequency calculation is slightly imprecise...");
  } else Serial.println(F("Uh oh... Not a match!"));
}

void checkBW() {
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_1);
  uint8_t x = vv >> 4;
  Serial.print(" . BW: " + String(x) + " [" + String((float)myBWs[x], 3) + " KHz]");
  Serial.print(F(" ---> "));
  if (bw == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
}

void checkSF() {
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_2);
  uint8_t x = vv >> 4;
  Serial.print(" . SF: " + String(x));
  Serial.print(F(" ---> "));
  if (sf == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
}

void checkCR() {
  uint8_t vv = LoRa.readRegister(REG_MODEM_CONFIG_1);
  uint8_t x = ((vv & 0b00001110) >> 1) + 4;
  Serial.print(" . CR: 4/" + String(x));
  Serial.print(F(" ---> "));
  if (cr == x) Serial.println(F("All good..."));
  else Serial.println(F("Uh oh... Not a match!"));
}

bool checkPWD() {
  uint8_t xCount = 0;
  for (uint8_t i = 0; i < 16; i++) xCount += myPWD[i];
  needAES = (xCount != 0);
  return (xCount != 0);
}

void hex2array(char *src, uint8_t *dst, size_t sLen) {
  size_t i, n = 0;
  for (i = 0; i < sLen; i += 2) {
    uint8_t x, c;
    c = src[i] - 48;
    if (c > 9) c -= 55;
    x = c << 4;
    c = src[i + 1] - 48;
    if (c > 9) c -= 55;
    dst[n++] = (x + c);
  }
}

void array2hex(uint8_t *buf, size_t sLen, char *x) {
  size_t i, len, n = 0;
  const char *hex = "0123456789abcdef";
  for (i = 0; i < sLen; ++i) {
    x[n++] = hex[(buf[i] >> 4) & 0xF];
    x[n++] = hex[buf[i] & 0xF];
  }
  x[n++] = 0;
}

void writeRegister(uint8_t reg, uint8_t value) {
  LoRa.writeRegister(reg, value);
}
uint8_t readRegister(uint8_t reg) {
  return LoRa.readRegister(reg);
}
