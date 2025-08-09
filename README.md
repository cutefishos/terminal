# CutefishOS Terminal

A terminal emulator for Cutefish.

### Third Party Code

[qmltermwidget](https://github.com/Swordfish90/qmltermwidget).

## Dependencies
Install dependencies
```sh
cd terminal
sudo apt build-dep .
```
Or alternatively

```sh
sudo apt install extra-cmake-modules qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev qttools5-dev
```

## Build and install
```sh
mkdir build
cd build
cmake ..
make
make install
```

## License

This project has been licensed by GPLv3.
