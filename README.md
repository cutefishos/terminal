# CutefishOS Terminal

A terminal emulator for Cutefish.

### Third Party Code

The emulator core comes from Konsole by way of
[qmltermwidget](https://github.com/Swordfish90/qmltermwidget).

## Dependencies

Terminal is built on [cutefish-toolkit](https://github.com/cutefishos/cutefish-toolkit);
install it first.

Debian/Ubuntu Dependencies:
```sh
sudo apt install cmake pkg-config libicu-dev libpcre2-dev libxkbcommon-dev
```

## Build and install
```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
make
sudo make install
```

## License

This project has been licensed by GPLv3.
