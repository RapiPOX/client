# RapiPOX

> **Rap**ax **P**oint **O**f e**X**tensions.
 
> Hardware dispositive with multiple extensions (in progress) realted with Lightning Network and NOSTR.

---

1. [Images](#images)
2. [Hardware Requirements](#hardware-requirements)
    1. [Electronic](#electronic)
    2. [Case and others](#case-and-others)
3. [Setup](#setup)
    1. [Development Enviroment Setup](#development-enviroment-setup)
    2. [Hardware Setup](#hardware-setup)
4. [Developing](#developing)
    1. [Code](#code)
    2. [Others](#others)

---

## Images

![RapiPOX](./images/RapiPOX_v1.0.0_La_Crypta.jpg 'RapiPOX in La Crypta')

## Hardware Requirements

### Electronic

1. ESP-WROOM-32.
2. Display OLED 0.96" 128x64 pixels.
3. NFC module PN532 13.56MHz.
4. x3 LEDs (I use red, yellow and green).
5. Buzzer.

### Case and others

1. 3D case. (files available soon).
2. x12 screw 1/8x3/8 inchs and its correspondin nut, and x2 additional nuts for touch screw.
3. USB cable.
4. Protoboard 400 points.
5. Several protoboard wires (Male-Male and Male-Female).
> In the future would I like to make a PCB.

## Setup

Setting up this project entails two main parts:

1. setting up your development evoirement using [PlatformIO IDE](https://platformio.org/) extension in VSCode, and
2. hardware setup

### Development Enviroment Setup 

1. Install [PlatformIO extension](https://platformio.org/install/ide?install=vscode) in VSCode. I recomend looking at this [Quick Start Guide](https://docs.platformio.org/en/latest/integration/ide/vscode.html#quick-start).
2. Clone this repo within `~/PlatformIO/Projects/` directory.
3. Import the project from _PlatformIO: Home_ and open it.

#### `env.hpp` Setup

1. Copy `env.hpp.example` content to `env.hpp` (create it).
2. Change the placeholder values (ie. `*ENV_...` strings):
    1. replace `*ENV_SSID` for you WiFi SSID. **It is important that the network is 2.4GHz** (5GHz networks don't are detected),
    2. replace `*ENV_PASS` for you WiFi password,
    3. replace `*ENV_LNURL` for your [LNURL](https://github.com/lnurl/luds/blob/luds/16.md), included domain. (ie. `user@domain`). Obtain your user@lawallet.ar [here](https://lawallet.ar/).

### Hardware Setup

Work in progress...

## Developing

### Code

#### Done

- [x] Turn off NTAG424 debug.
- [x] LEDs and status buzzer.
- [x] Customizable amount of invoice.
- [x] Make a README as [mpr](https://github.com/mariano-perez-rodriguez) would like.

#### To Do

- [ ] Beautifuly code.
- [ ] Error handling.
- [ ] Fix limits of the invoice depend to LNURL.
- [ ] Replace HTTP request for Websocket connection to my own sever.
- [ ] Increment invoice amount of 100 in 21. (eg. 100, 121, 200, 221, 300, ...).
- [ ] Acelerate increment and decrement invoice amount.

### Others

- [ ] Add 3D files and print configuration.
- [ ] Continue [Hardware Setup](#hardware-setup).
    - [ ] Conection planes.
- [ ] Make a PCB for the electronic circuit.
- [ ] Contribution guidlines

## Donations and community

If you would like to contribute with this project you can send donation to the following LNURL (obviously) ⚡rapax@lawallet.ar⚡.

This project born in  [La Crypta](https://lacrypta.ar/), argentinian bitcoiner community. Your are invited to join a [our Discord](https://discord.lacrypta.ar) to continue the talk of this project and build more.
