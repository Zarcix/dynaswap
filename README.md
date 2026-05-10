# DynaSwap

DynaSwap provides dynamic swap on Linux. It comes in two flavors:

- **[src/service/](src/service/)** — a userspace daemon that watches PSI
  memory pressure and adds or removes real swap files on the fly.
- **[src/driver/](src/driver/)** — a kernel block driver that exposes
  `/dev/dynaswap`, a virtual block device backed by a sparse file that grows
  on demand.

See the README in each directory for build and usage instructions.

