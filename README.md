# 4 player morse racer

4 players racing to beat each other at morse code transmission using real morse keys.

## Setup

- build host sw in `game/` using `make`
- connect 4 morse keys to an arduino-compatible board on pullup-capable non-serial pins. The keys should connect between pin and GND
- update pin assignments in `fw/firmware.ino`
- compile & upload firmware
- connect device to host
- start game using `./game TEXT [serial port] [baudrate]`. TEXT should only contain letters, no symbols numbers or spaces allowed (they get filtered out). Serial port defaults to `/dev/ttyUSB0` and baudrate defaults to 9600

## Game
The players must transmit the selected word using morse code on their key. The progress is displayed on the screen (highlighting the next letter to key, and printing it's progress). When a player finishes, their time (measured from the first keystroke in the game) is displayed alongside their place (1st, 2nd, 3rd, 4th)

## Credits

Built in 2024 by HA7DN, based on work of previous unknown HA5KFU members.

Serial code on the host side was based on the following article: https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/