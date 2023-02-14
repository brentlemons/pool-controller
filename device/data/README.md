# SPIFFS Data

The data in this directory will be written to memory on the chip using esptool. 

> SPIFFS has a max filename size of 32 characters.

- ca.pem - The AWS CA
- cert.pem - the device's cert
- private.key.pem - device's private key

From root of the project:

```console
mkspiffs -c ./device/data/ -s 1441792 -p 256 -b 4096 ./bin/pool-controller-device.spiffs.bin
esptool --chip esp32 -p /dev/cu.SLAB_USBtoUART -b 921600 --before default_reset --after hard_reset write_flash 2686976 ./bin/pool-controller-device.spiffs.bin
```

The values for mkspiffs and esptool are specific to the chipset. More information on determining values can be found in [this GitHub issue](https://github.com/esp8266/arduino-esp8266fs-plugin/issues/51).