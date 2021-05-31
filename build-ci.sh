#!/usr/bin/env bash

sudo apt-get update
sudo apt-get install -y gdebi-core g++-multilib
dkp-pacman -Syyu --noconfirm gba-dev
make -j7
