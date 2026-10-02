# rcn

Capture `event` files and send the input data to a `server`, 
which replays those inputs

This process can be `paused`, `resumed`, `stopped` and `killed` and any time 
from either the client or the server

Also a list of `event` files which should be captured, can be supplied

## server 

The server listens on a port for a connection, when first connecting, a little
handshake is completed, to exchange information about all devices

## client

The client grabs every device listed, then connects to the server

1. the client sends a list of every device with their functionality
2. the server replies when it created all virtual devices
3. the client now sends the input data to the server where it is replayed

## daemon mode

Both server and client create a local unix socket for IPC
This way new invocations of the program can `pause`, `resume`, ... the connection
There is no hard limit on the amount of connected processes to the socket
