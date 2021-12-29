#!/bin/sh

VERSION="1.0-0"

pkg .

mkdir -p ./usb-fanctl_${VERSION}_amd64/DEBIAN
cp ./.deb/control ./usb-fanctl_${VERSION}_amd64/DEBIAN

mkdir -p ./usb-fanctl_${VERSION}_amd64/usr/local/bin
cp ./dist/usb-fanctl-linux ./usb-fanctl_${VERSION}_amd64/usr/local/bin/usb-fanctl

dpkg-deb --build --root-owner-group usb-fanctl_${VERSION}_amd64

rm -rf ./usb-fanctl_${VERSION}_amd64
