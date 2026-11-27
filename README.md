# RCN

> [!WARNING]
>
> The network traffic is not encrypted and somebody could spy on your inputs.\
> This includes keystrokes, making it possible to capture passwords and such.

---

A remote control program for Linux to read input data from `eventX` files of the `client` and replay them on the `server`.

<p align="center">
    <a href="#installation">Installation</a> - 
    <a href="#quick-start-guide">Quick start</a> - 
    <a href="docs/USAGE.md">Documentation</a>
</p>

<p align="center">
  <a href="https://github.com/kapth09/rcn/releases"><img src="https://img.shields.io/github/downloads/kapth09/rcn/total?color=1f293b" alt="Downloads"></a>
</p>
</br>

## Installation

### Using a package manager

Download the right package from the `releases` page and install it with your package manager.

Supported package formats are:

- `.deb`, for Debian based Distros
- `.rpm`, for RHEL based Distros
- `.tar.zst`, for Arch based Distros

### Manual install

Clone the repo:

```bash
git clone https://github.com/kapth09/rcn.git
```

> Ensure `gcc` and `make` are installed.

And install:

```bash
make install
```

<br>

## Uninstall

### Installed as a package

Run the `"remove"` command for the package manager of your Distro.

### Manually installed

Run the `uninstall` Make target:

```bash
make uninstall
```

<br>

## Quick start guide

Simplified usage guide for the program.

> [!NOTE]
>
> This program reads from `/dev/input/eventX` files and writes to `/dev/uinput`, which require special permissions. Being a member of the `input` group fixes this problem on some Distros.

#### Show available devices

```Bash
rcn show
```

#### Start the server

```Bash
rcn start --port <port>
```

#### Connect to a server

```Bash
rcn connect --server <server-addr> --port <port> --devices </dev/input/eventX>
```

#### Pause

```Bash
rcn pause
```

#### Resume

```Bash
rcn resume
```

#### Toggle between paused and resumed state

```Bash
rcn switch
```

#### Show program status

```Bash
rcn ls
```

#### Dynamically configure devices

**Add new devices**

```Bash
rcn add -d </dev/input/eventX> ...
```

**Remove devices**

```Bash
rcn rm -d <id> ...
```

**Pause/Resume specific devices**

```Bash
rcn mod -d <id> ...
```
