# RCN

A remote control program for Linux to read input data from `eventX` files of the `client` and replay them on the `server`.

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

#### Start

To start the serve on the given port (`-p/--port`) and listen for a  client to connect and replay input data. 

```
rcn start -p <port>
```

#### Connect

Connect to the specified server (`-s/--server`) on the given port (`-p/--port`). `-d/--devices` accepts one or more absolute paths to  `eventX` files. The program grabs the devices, so their input isn't captured on the client.

```bash
rcn connect -p <port> -s <server-address> -d </dev/input/eventX> ...
```

#### Subactions

The program runs in the background as a "daemon" and can be interacted via `subactions`. For a `subaction` you can specify the target, `server` or `client`. This is useful if one computer is a server and a client at the same time.

```bash
rcn server <subaction>
rcn client <subaction>
```

If `server`/`client` is omitted, it checks which daemon is running and sends the `subaction` to it. It defaults to `client` if both server and client are running.

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



