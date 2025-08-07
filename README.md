# DynaSwap

Welcome to dynaswap! Dynaswap allows you to set up dynamic swap on Linux distributions.

## Building

0. Dependencies
  - libconfig
  - procps
1. Run `make`
2. That's it

## Running

There are currently two ways to run this program
1. Command Line
  - Make sure that you have built the application
  - `sudo ./build/<arch>/dynaswap -c misc/dynaswap.conf`
  - Sudo is required since this directly creates swap files and swaps the files on.
2. Systemd Service
  - The systemd service file is declared in `misc/dynaswap.service`
  - Installation of the service is required.
  - The file assumes that dynaswap's config file lies in `/etc/dynaswap.conf`

## Configuratino

Dynaswap uses a configuration file to start up the program. A default configration file can be found in `misc/dynaswap.conf`
