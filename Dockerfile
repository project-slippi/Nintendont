FROM devkitpro/devkitppc

MAINTAINER Nikki <nikki@slippi.gg>

RUN dkp-pacman -Syyu --noconfirm gba-dev && \
    dkp-pacman -S --needed --noconfirm `dkp-pacman -Slq dkp-libs | grep '^3ds-'` && \
    dkp-pacman -S --needed --noconfirm `dkp-pacman -Slq dkp-libs | grep '^ppc-'` && \
    dkp-pacman -Scc --noconfirm
RUN apt-get update
RUN apt-get install -y "g++-multilib"
ENV DEVKITARM=${DEVKITPRO}/devkitARM