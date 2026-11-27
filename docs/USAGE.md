# Detailed documentation and explanation for RCN

RCN is a remote control program for any Linux based operating system written in C. It allows the user to capture the inputs from one device (the client) and replay them on another computer (the server).

## Tested devices

A list of every tested device and proven to work:

- Keychron K3 Max
- Roccat Kone Aimo 16K
- Chicony USB-Keyboard
- Nintendo Switch 1 Pro-Controller
- GAOMON S620 Graphics Table
- Acer Nitro 5 Laptop
  - Keyboard
  - Touchpad
- Dell XPS 9440
  - Keyboard

> [!NOTE]
>
> This program reads from `/dev/input/eventX` files and writes to `/dev/uinput`, which require special permissions. For some Distros, like Fedora, it's enough to be a member of the `input` group. Other Distros, like Debian, are stricter and require super-user privileges.



## CLI

The CLI is split into `actions` and `subactions`. This is because, the program runs in the background as a daemon. Certain flags are also supported.

### CLI - Flags

- `-s/--server`: IP-Address or hostname of the server
- `-p/--port`: Port number of the server
- `-d/--devices` : A list of paths to `eventX` device files
- `-f/--filter`: Filter devices by their input type
  - Possible filters are: **key, abs, rel, swt**
- `-h/--help`:  Print the help message. A list of (sub)actions can supplied for more detailed info.
- `-g/--debug`: Adds extra debug info to the daemon's log



### CLI - Actions

Actions are general commands, such as starting the program or useful commands to inspect the computer.

#### Start

Start the server on a port.

```Bash
rcn start --port <port>
```

#### Connect

Connect to the server and transmit the devices

```Bash
rcn connect --server <server-addr> --port <port> --devices /dev/input/eventX ...
```

#### Show

Show available devices for capture, including their `event-types`, `eventX` file and `device name`.

```Bash
rcn show
```

The `-f/--filter` flag works for this action and only prints devices which have all of the event types.

```Bash
rcn show --filter key abs # Only prints devices with both key and abs event types
[KEY,ABS]	event8		ELAN050A:01 04F3:3158 Touchpad
```



### CLI - Subactions

#### Server and Client

Specifies whether the following subaction is sent to the client daemon or the server daemon. If this specification is omitted, it picks whatever daemon is running, or the client daemon if both are running. This can be applied to all subactions, but will be left out for the sake of brevity.   

```Bash
rcn server <subaction> # target specifically the server daemon
rcn client <subaction> # target specifically the client daemon
rcn <subaction> # automatically choose a daemon, client if both are running
```

#### Pause

Pause the connection, device inputs are released no longer replayed on the server.

```Bash
rcn pause
```

#### Resume

Resume the connection, devices are re-captured and inputs are replayed on the server.

```Bash
rcn resume
```

#### Stop

Stop the connection. If the client daemon receives this subaction, it disconnects from the server and the server is available for a new client. If the server daemon receives this subaction, any connected client is disconnected, virtual devices are deleted and the daemon exits.

```Bash
rcn stop
```

#### Switch

Switch between the pause and resume state.

```Bash
rcn switch
```

#### Log

Print the daemon's logs to `stdout`. 

```Bash
rcn log
```

When no daemon is running, you can still view the daemon's logs by specifying the target (server/client).

#### List

Show a list of all devices in the rcn context. This prints their unique ID, their state and their name.

```Bash 
rcn list
rcn:	35799	[released]	'ELAN050A:01 04F3:3158 Touchpad'
rcn:	2060	[captured]	'AT Translated Set 2 keyboard'
```

#### Add

Dynamically add devices while already connected. Currently this command needs to be run from the client.

```Bash
rcn add --devices /dev/input/eventX ...
```

#### Remove

Dynamically remove devices while already connected. The `--devices` flag now takes the `id` of the devices. Currently this command needs to be run from the client.

```Bash
rcn rm --devices <id> ...
```

#### Mod

Dynamically modify the state of specific devices.  The `--devices` flag now takes the `id` of the devices. Currently this command needs to be run from the client.

```Bash
rcn mod --devices <id> ...
```

