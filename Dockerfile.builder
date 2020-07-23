FROM debian:buster

RUN apt update \
  && apt install -y gcc meson libpango1.0-dev libcairo2-dev libsystemd-dev libgdk-pixbuf2.0-dev libwayland-dev wayland-protocols libelogind-dev scdoc

WORKDIR /mako

CMD meson build && ninja -C build
