# RCN

Table of Contents

- [About](#about)
- [Tested devices](#tested-devices)
- [Install](#install)
    - [Using a package manager](#using-a-package-manager)
    - [Using Make](#using-make)
- [Uninstall](#uninstall)
    - [Installed via package](#installed-via-package)
    - [Installed via Make](#installed-via-make)
- [Usage](#usage)
    - [Actions](#actions)
      - [Start](#start)
      - [Connect](#connect)
      - [Show](#show)
    - [Subactions](#subactions)
      - [Pause](#pause)
      - [Resume](#resume)
      - [Stop](#stop)
      - [Log](#log)
      - [List / ls](#list--ls)


## About

A remote control program for Linux to read input data from `eventX` files of the `client` and replay them on the `server`.

> [!WARNING]
>
> All network traffic is unencrypted and somebody could be spying on your inputs.  
> This includes keystrokes, making it possible to capture your password.

## Tested devices

A list of every tested device and proven to work:
- Keychron K3 Max
- Roccat Kone Aimo 16K
- Chicony USB-Keyboard
- Nintendo Switch 1 Pro-Controller
- Acer Nitro 5 Laptop
    - Keyboard
    - Touchpad

## Install

#### Using a package manager

Download the right package from the `releases` page and install it with your package manager.

Supported package formats are:

- `.deb`, for Debian based Distros
- `.rpm`, for RHEL based Distros
- `.tar.zst`, for Arch based Distros

#### Using Make

Clone the repo using:

```bash
git clone https://github.com/kapth09/rcn.git
```

> Ensure `gcc` and `make` are installed.

To install run:

```bash
make install
```

## Uninstall

#### Installed via package

Run the `"remove"` command for the package manager of your Distro.

#### Installed via Make

Run the `uninstall` Make target:

```bash
make uninstall
```

## Usage

> [!NOTE]
>
> This program reads from `/dev/input/eventX` files and writes to `/dev/uinput`, which require special permissions. For some Distros, like Fedora, it's enough to be a member of the `input` group. Other Distros, like Debian, are stricter and require super-user privileges.

This program's CLI is split into `actions` and `subactions`.  
Actions are general commands, for example to start the program.  
Subactions are commands which need a target. The target is either `client` or `server`.

```bash
# action without target
rcn <action> ...
# subaction with target
rcn client/server <subaction> ...
```

This divide exists because the program runs in the background as a `daemon`.   
A computer can be both a `client` and a `server`, so there must be a way to destinguish between them.

However, subaction can ommitt the `target` and the program automatically checks which daemon is running.  
If both `client` and `server` are running, it defaults to the `client`.


#### Actions

##### Start

To start the serve on the given port (`-p/--port`) and listen for a  client to connect and replay input data. 

```
rcn start -p <port>
```

##### Connect

Connect to the specified server (`-s/--server`) on the given port (`-p/--port`). `-d/--devices` accepts one or more absolute paths to  `eventX` files. The program grabs the devices, so their input isn't captured on the client.

```bash
rcn connect -p <port> -s <server-address> -d </dev/input/eventX> ...
```

To find available devices, use the `show` action or view the symlinks in `/dev/input/by-id` and `/dev/input/by-path`.

##### Show

Prints for every `/dev/input/eventX` file, the full name of the device, the event capabilites (`EV_KEY`, `EV_REl`, `EV_ABS`, `EV_SW`) and the specific eventX file-name.

```bash
rcn show
```

Output:

|Type           |eventX |Full device name                          |
|---------------|-------|------------------------------------------|
|[KEY] 	        |event2 |Power Button                              |
|[KEY, REL, ABS]|event11|Keychron  Keychron Link  Keyboard         |   
|[KEY, REL]     |event3 |ROCCAT ROCCAT Kone Aimo 16K Mouse         |

> [!NOTE]
>
> The driver/firmware of the device can report more event types than the real physical device actually has. This may lead to confusing output such as a normal keyboard having `KEY`, `REL` and `ABS` as a event types.

This action also supports the `-f/--filter` flag. 
This lets you filter out any device which doesn't have at least all the specified event types.

Possible filters are:
- **key** (The device has buttons, like a keyboard)
- **rel** (The device has relative movement, like a mouse)
- **abs** (The device has absolute positions, like a touchpad)
- **swt** (The device has binary switches, like a laptop lid)

The case of the filter is ignored, so `KEY` and `kEy` are the same as `key`.

So with this command:

```bash
rcn show --filter rel abs
```

The list above gets filtered as:

|Type           |eventX |Full device name                          |
|---------------|-------|------------------------------------------|
|[KEY, REL, ABS]|event11|Keychron  Keychron Link  Keyboard         |   

#### Subactions

The program runs in the background as a "daemon" and can be interacted via `subactions`. For a `subaction` you can specify the target, `server` or `client`. This is useful if one computer is a server and a client at the same time.

```bash
rcn server <subaction>
rcn client <subaction>
```

If `server`/`client` is omitted, the program checks which daemon is running and sends the `subaction` to it. It defaults to `client` if both server and client are running.

##### Pause

Pause the capture of `eventX` files. The client un-grabs the devices and the server doesn't replay events anymore.

```bash
rcn pause
```

##### Resume

Resume the capture of `eventX` files. The client re-grabs the devices and the server resumes to replay event data.

```bash
rcn resume
```

##### Stop

If the `client` is stopped, it releases all devices and the daemon process exits. The `server` deletes all virtual devices and is ready for another `client` to connect.

If the `server` is stopped, it deletes all virtual devices and the `client` is also stopped and un-grabs the devices.

```bash
rcn stop
```

##### Log

Print the log of the running daemon. If no daemon is running, prefixing the command with `server`/`client` is necessary.

```bash
rcn log
```

```bash
rcn server log
```

##### List / ls

List all captured devices and their status, grabbed or ungrabbed. Should print the same for both `client` and `server`. `ls` is a alias for `list` and does the same thing.

```bash
rcn list
```

```bash
rcn ls
```
