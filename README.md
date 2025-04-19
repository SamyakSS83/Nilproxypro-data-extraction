### Installations:
```
sudo apt update
sudo apt install libqt5serialport5-dev
```

### Building:

```
cd {path to esp_reader}
mkdir build
cd build
cmake ..
ls #to verify that Makefile is generated
make
```

### Usage:
```
./ESPReader
```