## Flashing firmware

Best done with a Raspberry Pi or another Pico you have available.

With a Raspberry Pi you can use [OpenOCD](https://openocd.org/) to flash the main board over SWD using two GPIO pins, e.g. 23 and 24. The following config should be saved as `openocd.cfg`.

```ini
source [find interface/raspberrypi-native.cfg]

adapter gpio swclk 23
adapter gpio swdio 24

set CHIPNAME rp2040
source [find target/rp2040.cfg]
adapter speed 100 

init
targets
reset halt
program picoave.elf verify
reset run
shutdown
```

OpenOCD can then be started by `sudo openocd` in the same directory. Flashing will only take a few seconds.

### Connections

| Signal | RPi Pin |
| ------ | :-----: |
| SWCLK  | 23      |
| SWDIO  | 24      |
| GND    | any GND |