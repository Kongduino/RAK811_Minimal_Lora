#undef max
#undef min
#include <string>
#include <vector>
#include <cstring>

using namespace std;
#include <SPI.h>
#include <LoRa.h>
#include "LoRaHelper.h"
#include "Commands.h"

int counter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  cmdCount = sizeof(cmds) / sizeof(myCommand);
  Serial.println("RAK811 Minimal LoRa");
  Serial.print(" - cmdCount: "); Serial.println(cmdCount);
  // https://github.com/stm32duino/wiki/wiki/lora
  pinMode(RADIO_XTAL_EN, OUTPUT); //Power LoRa module
  digitalWrite(RADIO_XTAL_EN, HIGH);
  // void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
  // L100C6Ux(A)_L151C(6-8-B)(T-U)x(A)_L152C(6-8-B)(T-U)x(A) / variant_RAK811_TRACKER.h
  // nss = 26 - aka RADIO_NSS PB0
  // rxtx = 32 - aka RADIO_RF_CRX_RX PB6
  // rst = 21 - aka RADIO_RESET PB13
  // PA BOOST = 34 - AKA RADIO_RF_CTX_PA PA4
  // dio0/1/2 = 27, 28, 29
  //  RADIO_DIO_0 PA11 27
  //  RADIO_DIO_1 PB1 28
  //  RADIO_DIO_2 PA3 29
  //  RADIO_DIO_3 PH0 30
  //  RADIO_DIO_4 PC13 31
  // https://github.com/RAKWireless/RAK811_LoRaWAN_Arduino
  LoRa.setPins(RADIO_NSS, RADIO_RESET, RADIO_DIO_0);
  if (!LoRa.begin(myFreq)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  } else {
    Serial.print(" - Started LoRa @ ");
    Serial.print(myFreq);
    Serial.println(" MHz.");
#if defined(TRUST_BUT_VERIFY)
    checkFreq();
#endif
  }
  LoRa.sleep();
  LoRa.setTxPower(txPower, PA_OUTPUT_PA_BOOST_PIN);
  Serial.print(" - txPower: "); Serial.println(txPower);
  LoRa.setPreambleLength(preamble);
  Serial.print(" - preamble: "); Serial.println(preamble);
  LoRa.setSpreadingFactor(sf);
  Serial.print(" - SF: "); Serial.println(sf);
#if defined(TRUST_BUT_VERIFY)
  checkSF();
#endif
  LoRa.setSignalBandwidth(myBWs[bw] * 1e3);
  Serial.print(" - BW: "); Serial.print(bw);
  Serial.print(" / "); Serial.print(myBWs[bw]); Serial.println(" KHz");
#if defined(TRUST_BUT_VERIFY)
    checkBW();
#endif
  LoRa.setCodingRate4(cr);
  Serial.print(" - CR 4/"); Serial.println(cr);
#if defined(TRUST_BUT_VERIFY)
    checkCR();
#endif

  Serial.println("\n\nAES Test!");
  uint8_t pKey[16] = {0};
  uint8_t IV[16] = {0};
  uint8_t pKeyLen = 16;
  uint8_t randomBuff[64] = {
    0x5f, 0x21, 0x62, 0x02, 0xf8, 0xe7, 0x4c, 0x4c,
    0xa7, 0xb7, 0x69, 0xae, 0x78, 0x5f, 0x21, 0xd6,
    0x5a, 0x1f, 0x38, 0xd8, 0xae, 0x80, 0x4e, 0x4b,
    0xad, 0x2e, 0x41, 0x89, 0xa3, 0x62, 0x08, 0x2b,
    0x6c, 0x59, 0xc9, 0x10, 0x7c, 0x09, 0x48, 0x03,
    0x8c, 0x66, 0x36, 0xe6, 0x4e, 0xc8, 0x7b, 0x53,
    0x53, 0x57, 0xd8, 0x59, 0x75, 0x3c, 0x4d, 0xd1,
    0xa6, 0x63, 0x15, 0x8f, 0x81, 0x6b, 0x5b, 0x19
  };
  memcpy(pKey, randomBuff, 16);
  Serial.println("pKey:");
  hexDump(pKey, 16);
  memcpy(IV, randomBuff + 16, 16);
  Serial.println("IV:");
  hexDump(IV, 16);
  strcpy(msg, (char*)"Hello user! This is a plain text string!");
  int ln = strlen(msg) + 1;
  Serial.println("Plain Text:");
  hexDump((uint8_t*)msg, ln);
  counter = 0;
  uint32_t t0 = millis();
  int16_t olen;
  while (millis() - t0 < 1000) {
    olen = encryptCBC((uint8_t*)msg, ln, pKey, IV);
    counter++;
  }
  Serial.print("CBC Encoded: ");
  Serial.print(ln);
  Serial.print(" vs ");
  Serial.println(olen);
  hexDump((unsigned char *)encBuf, olen);
  Serial.printf("%d round / s\n", counter);

  memcpy(msg, encBuf, olen);
  counter = 0;
  t0 = millis();
  while (millis() - t0 < 1000) {
    decryptCBC((uint8_t*)msg, olen, pKey, IV);
    counter++;
  }
  Serial.print("CBC Decoded: ");
  Serial.println(olen);
  hexDump((unsigned char *)encBuf, olen);
  Serial.printf("%d round / s\n", counter);

  listenMode();
}

void loop() {
  if (apFreq > 0) {
    if (millis() - lastPing >= apFreq) handlePing("");
  }
  int packetSize = LoRa.parsePacket();
  if (packetSize) onReceive(packetSize);
  // DIO0 callback doesn't seem to work,
  // we have to do it by hand for now anyway
  if (Serial.available()) {
    // incoming from user
    char incoming[256];
    memset(incoming, 0, 256);
    uint8_t ix = 0;
    while (Serial.available()) {
      char c = Serial.read();
      delay(25);
      if (c == 13 || c == 10) {
        // cr / lf: we want to buffer lines and treat them one by one
        // when we're done receiving.
        if (ix > 0) {
          // only if we have a line to save:
          // if we receive CR + LF, the second byte would give us
          // an empty line.
          incoming[ix] = 0;
          string nextLine = string(incoming);
          userStrings.push_back(nextLine);
          ix = 0;
        }
      } else incoming[ix++] = c;
    }
    // if you don't terminate your last command with CR and/or LF,
    // not my problem... ;-)
    // But seriously I should add whatever is left to userStrings...
  }
  if (userStrings.size() > 0) {
    uint8_t ix, iy = userStrings.size();
    for (ix = 0; ix < iy; ix++) {
      handleCommands(userStrings[ix]);
    }
    userStrings.resize(0);
  }
}