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
  Serial.print("Received packet '");
  // read packet
  for (int i = 0; i < packetSize; i++) {
    Serial.print((char)LoRa.read());
  }
  // print RSSI of packet
  Serial.print("' with RSSI ");
  Serial.println(LoRa.packetRssi());
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

void hexDump(uint8_t* buf, uint16_t len) {
  // Something similar to the Unix/Linux hexdump -C command
  // Pretty-prints the contents of a buffer, 16 bytes a row
  char alphabet[17] = "0123456789abcdef";
  uint16_t i, index;
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
  Serial.print(F("   |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |\n"));
  for (i = 0; i < len; i += 16) {
    if (i % 128 == 0) Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
    char s[] = "|                                                | |                |\n";
    // pre-formated line. We will replace the spaces with text when appropriate.
    uint8_t ix = 1, iy = 52, j;
    for (j = 0; j < 16; j++) {
      if (i + j < len) {
        uint8_t c = buf[i + j];
        // fastest way to convert a byte to its 2-digit hex equivalent
        s[ix++] = alphabet[(c >> 4) & 0x0F];
        s[ix++] = alphabet[c & 0x0F];
        ix++;
        if (c > 31 && c < 127) s[iy++] = c;
        else s[iy++] = '.'; // display ASCII code 0x20-0x7F or a dot.
      }
    }
    index = i / 16;
    // display line number then the text
    if (i < 256) Serial.write(' ');
    Serial.print(index, HEX); Serial.write('.');
    Serial.print(s);
  }
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
}
