#!/bin/sh
# Query the linker version
ld --version || true

# Query the (g)libc version
ldd --version || true

# Install packages via pacman
pacman -Syyu --noconfirm
pacman -S --noconfirm $(cat "dependencies/archlinux:20200306.txt")
