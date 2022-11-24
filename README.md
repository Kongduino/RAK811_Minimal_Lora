# RAK811_Minimal_Lora

The RAK811 *was* a hot product back when I discovered RAKwireless, as a customer, then a friend of the CEO and other team members. I have a few of them, in various frequency ranges (with a strong bias for 470 MHz), and still use them. They are loaded with an [AT firmware](https://downloads.rakwireless.com/LoRa/RAK811/Firmware/), which has been regularly updated until late 2020. So mostly I have been using these, whether the [Arduino shield](https://www.thethingsnetwork.org/docs/devices/rak811-wisnode-lora-module/) – which was discontinued, but you can find traces of it on the web – or the [breakout board](https://store.rakwireless.com/products/rak811-lpwan-breakout-module), as devices tethered to another machine, my Mac, some of my RPi, etc. I have a bunch of apps, from a Xojo GUI app to a headless Python script, that handle them well.

![Shield](assets/Shield.jpg)

![Breakout](assets/Breakout.jpg)

When I joined I was told *You can't reprogram it*. Which of course I took the wrong way! Turns out, yeah, back then you needed Keil to program it and use the LoRaWAN stack. But since then, there's an [Arduino BSP for STM32 devices](https://github.com/stm32duino/Arduino_Core_STM32), and although it took a while, there is now a way to use the LoRa chip inside the RAK811, an SX1276, with your library of choice. For LoRa, I use Sandeep Mistry's library, which I know well and have patched to make it work slightly better. For this example to work, you don't need to use my version, but you do need to effect a small patch:

Lines 109-110 in the original source code of the header file must be moved **before** `private` on line 97:

```c
[...]
  void dumpRegisters(Stream& out);

  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);

private:
  void explicitHeaderMode();
[...]
```

These 2 functions are required to play a little bit of magic on registers, and verify settings are actually set properly.

## Turning on the SX1276, and the Rx/Tx switch

With this done, you have everything to run Minimal LoRa on RAK811 devices. The settings required to make it work are mostly in a separate header file, `LoRaHelper.h`, plus on [lines 22-23](https://github.com/Kongduino/RAK811_Minimal_Lora/blob/master/RAK811_Minimal_Lora.ino#L22-L23) of the ino sketch:

```c
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

[...]

  pinMode(RADIO_XTAL_EN, OUTPUT); //Power LoRa module
  digitalWrite(RADIO_XTAL_EN, HIGH);
```

## Minimal Lora Commands

This version of Minimal LoRa comes with a subset of the commands available in other versions. It does come with my new commands engine, which is way easier to use and extend.

```
Available commands: 6
 . help: Shows this help.
 . fq: Gets/sets the working frequency.
 . bw: Gets/sets the working bandwidth.
 . sf: Gets/sets the Mspreading factor.
 . cr: Gets/sets the working coding rate.
 . p: Sends a ping packet.
```

Commands start with a `/`, so `/fq 469.125`, `/bw 6`, etc. Anything else is considered as a message to be sent.

On the subject of BW, the bandwidth, and SF, the spreading factor. Contrary to LoRaWAN, which uses only a subset of LoRa's BW range, and only in a limited number of SF/BW combinations, in LoRa you can do (at least technically, what the law in your country might be... different) pretty much what you want, and use the whole range of BW settings:

```c
float myBWs[10] = {7.8, 10.4, 15.63, 20.83, 31.25, 41.67, 62.5, 125, 250, 500};
```

You can select independently any of the 10 BW values, `/bw 0` to `/bw 9`, and SF 6 to 12, knowing that `SF 6` is for special uses.

## The `TRUST_BUT_VERIFY` directive

If you have doubts about settings being applied properly, uncomment the `TRUST_BUT_VERIFY` define, in `LoRaHelper.h`, and recompile. When setting a new SF, BW, CR or frequency, the code will verify by reading the LoRa registers – hence the need, among other reasons, for the patch to the library...

## AES

I tested, separately, that AES worked on the RAK811 – no reason it shouldn't, but... – and will add commands to set up a key, Iv (NEVER reuse an Iv, or use a predictable Iv!), and switch AES on/off. HMAC could also be added at some point.

That's it for now, but it is already quite usable for range tests. Happy LoRaWalks!
