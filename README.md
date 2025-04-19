### Installations:
```
sudo apt update
sudo apt-get install libqt5serialport5-dev
```

### Building From Source:

```
cd {path to esp_reader}
mkdir build
cd build
qmake ..
ls #to verify that Makefile is generated
make
```

### Usage:
```
./ESPReader
```