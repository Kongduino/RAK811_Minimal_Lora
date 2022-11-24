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
  }
  LoRa.sleep();
  LoRa.setTxPower(txPower, PA_OUTPUT_PA_BOOST_PIN);
  Serial.print(" - txPower: "); Serial.println(txPower);
  LoRa.setPreambleLength(preamble);
  Serial.print(" - preamble: "); Serial.println(preamble);
  LoRa.setSpreadingFactor(sf);
  Serial.print(" - SF: "); Serial.println(sf);
  LoRa.setSignalBandwidth(myBWs[bw] * 1e3);
  Serial.print(" - BW: "); Serial.print(bw);
  Serial.print(" / "); Serial.print(myBWs[bw]); Serial.println(" KHz");
  LoRa.setCodingRate4(cr);
  Serial.print(" - CR 4/"); Serial.println(cr);
  listenMode();
}

void loop() {
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
  }
  if (userStrings.size() > 0) {
    uint8_t ix, iy = userStrings.size();
    for (ix = 0; ix < iy; ix++) {
      handleCommands(userStrings[ix]);
    }
    userStrings.resize(0);
  }
}
