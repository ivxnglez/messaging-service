# Messaging service

A small WhatsApp-like messaging system built for the Distributed Systems course at UC3M (2025-2026). Users register, connect to a central server and send each other text messages. If the recipient is offline the server keeps the message and delivers it when they connect. In part 2 users can also attach files, which are transferred directly between clients.

The server is written in C and the client in Python. Three technologies are involved:

- **TCP sockets** for everything client/server, plus the direct client-to-client file transfer.
- **ONC-RPC** for a logging service that prints every operation users perform.
- **A REST web service** (FastAPI) that collapses repeated whitespace in messages before they are sent.

## Layout

```
parte1/                  basic messaging
  client/client.py
  server/                C server (Makefile, src/, database/)
parte2/                  part 1 plus attachments, web service and RPC
  client/client.py
  server/                C server, now also an RPC client
  rpc_server/            ONC-RPC log server (log_rpc.x + implementation)
  web_service/           ws_normalize.py, requirements.txt
```

`parte2` is a superset of `parte1`, so if you only want to try the messaging itself, start with part 1.

## How it works

Every operation uses its own short TCP connection: the client connects to the server, sends the operation name and its arguments, reads a one-byte result code and closes the socket. All fields are sent as null-terminated strings.

Each client process has two parts. The main thread runs the interactive shell and talks to the server. When you `CONNECT`, the client asks the OS for a free port, starts a listener thread on it and tells the server which port that is. From then on the server connects to that listener whenever it needs to push something: an incoming message, or a delivery confirmation for a message you sent. Only one user can be connected per client process.

The server accepts connections in a main loop and starts a detached thread per request. The accepted socket descriptor is handed to the worker through a shared variable guarded by a mutex and a condition variable, and the main thread waits until the worker has copied it before accepting again.

State lives in a `database/` directory next to where the server is started (so run it from its own folder):

- `usuarios.txt` has one line per user: name, connected flag, IP, port and the last message id used.
- `mensajes_<user>.dat` holds the pending messages for that user as fixed-size binary structs.

A single global mutex serialises all database access, and `usuarios.txt` is updated by writing a temp file and renaming it. Since everything is on disk, users and pending messages survive a server restart.

Message ids are per sender, stored as `unsigned int`. A new user starts at 0, so the first message is 1. After the maximum value the counter wraps to 0 and the next id is 1 again.

### Attachments (part 2)

`SENDATTACH` works like `SEND` but also carries a file name (an absolute path). The server only stores and forwards the name, never the contents. The receiver sees a `FILE <path>` line under the message and can download it with `GETFILE`. For that the client needs the other user's IP and port, which it gets from `USERS` (in part 2 the server returns each entry as `user :: ip :: port`). If the user isn't in the cached list the client refreshes it once before giving up.

The download itself is a plain socket exchange: the requester sends `GET_FILE`, its own username and the path to the owner's listener thread. The owner replies with a status byte (0 = found, 1 = not found) followed by the file in 4 KB chunks.

### Web service (part 2)

`ws_normalize.py` exposes `POST /normalize` taking `{"message": "..."}` and returning `{"normalized": "..."}`, plus a `GET /` health check. It replaces runs of whitespace with a single space. It binds to `127.0.0.1:5000`, and each client machine is expected to run its own copy. The client calls it before every `SEND` and `SENDATTACH`; if the service is down or takes more than 2 seconds the original message is sent as is.

### RPC logging (part 2)

`log_rpc.x` defines a single call, `LOG_OPERATION(usuario, operacion, fichero)`. After each user operation the messaging server calls it and the RPC server prints `user  OPERATION` (plus the file name for `SENDATTACH`). Message text is never sent. The server finds the RPC host through the `LOG_RPC_IP` environment variable; if it is unset or the connection fails, logging is simply disabled and the server carries on.

## Requirements

- Linux, gcc, make, pkg-config
- Part 2 only: `libtirpc-dev` and `rpcbind` (and `rpcgen` if you want to regenerate the stubs, which are already committed)
- Python 3 with `requests`; for the web service also `fastapi`, `uvicorn[standard]` and `pydantic`

On Debian/Ubuntu:

```bash
sudo apt install build-essential pkg-config libtirpc-dev rpcbind
```

## Building and running

### Part 1

```bash
cd parte1/server
make
./server -p 8888
```

In another terminal (start as many clients as you like):

```bash
cd parte1/client
python3 client.py -s localhost -p 8888
```

Ports must be between 1024 and 65535.

### Part 2

Start things in this order.

```bash
# RPC log server
cd parte2/rpc_server
make
sudo systemctl start rpcbind   # if it isn't running already
./log_rpc_server

# web service (one per client machine)
cd parte2/web_service
pip install -r requirements.txt
python3 ws_normalize.py

# messaging server
cd parte2/server
make
LOG_RPC_IP=localhost ./server -p 8888

# clients
cd parte2/client
pip install requests
python3 client.py -s <server-ip> -p 8888
```

`LOG_RPC_IP` is the host where the RPC server runs. Use `make clean` in any of the folders to remove build output. To start from a clean state, empty the server's `database/` folder.

If you run the pieces in separate containers or machines, remember the server has to be able to reach the clients' listener ports, which are ephemeral.

## Client commands

| Command | What it does |
|---|---|
| `REGISTER <user>` | create a user |
| `UNREGISTER <user>` | delete a user and their pending messages |
| `CONNECT <user>` | come online and start receiving messages |
| `DISCONNECT <user>` | go offline |
| `USERS` | list connected users |
| `SEND <user> <message>` | send a message (up to 255 characters) |
| `SENDATTACH <user> <message> <path>` | send a message with an attached file (part 2) |
| `GETFILE <user> <remote path> <local path>` | download a file from another user (part 2) |
| `QUIT` | exit |

Messages arriving from other users are printed by the listener thread:

```
MESSAGE 1 FROM ana
hello there
END
FILE /tmp/data.txt          <- only for attachments
```

When a message you sent has been delivered you get `SEND MESSAGE <id> OK` (or `SENDATTACH MESSAGE <id> <file> OK`).

## Example

```
# ana
c> REGISTER ana
REGISTER OK
c> CONNECT ana
CONNECT OK
c> SEND luis hello     there
SEND OK - MESSAGE 1
```

luis is offline, so the server logs `MESSAGE 1 FROM ana TO luis STORED`. Later:

```
# luis
c> REGISTER luis
REGISTER OK
c> CONNECT luis
CONNECT OK
MESSAGE 1 FROM ana
hello there
END
```

and ana's terminal prints `SEND MESSAGE 1 OK`. With an attachment:

```
# ana
c> SENDATTACH luis look /tmp/data.txt
SENDATTACH OK - MESSAGE 2

# luis
MESSAGE 2 FROM ana
look
END
FILE /tmp/data.txt
c> GETFILE ana /tmp/data.txt /tmp/copy.txt
GETFILE OK
```

## Protocol reference

Result codes are a single byte. Strings are null-terminated.

Client to server:

| Operation | Strings sent | Reply |
|---|---|---|
| REGISTER | `REGISTER`, user | 0 ok, 1 already exists, 2 error |
| UNREGISTER | `UNREGISTER`, user | 0 ok, 1 no such user, 2 error |
| CONNECT | `CONNECT`, user, listen port | 0 ok, 1 no such user, 2 already connected, 3 error |
| DISCONNECT | `DISCONNECT`, user | 0 ok, 1 no such user, 2 not connected, 3 error |
| SEND | `SEND`, sender, recipient, message | 0 ok followed by the id as a string, 1 recipient doesn't exist, 2 error |
| SENDATTACH | `SENDATTACH`, sender, recipient, message, file name | same as SEND |
| USERS | `USERS`, user | 0 ok followed by a count and that many strings, 1 not connected, 2 error |

Server to a client's listener thread:

| Operation | Strings sent |
|---|---|
| `SEND_MESSAGE` | sender, id, message |
| `SEND_MESSAGE_ATTACH` | sender, id, message, file name |
| `SEND_MESS_ACK` | id |
| `SEND_MESS_ATTACH_ACK` | id, file name |

If the server can't reach a client when delivering, it assumes that client went offline, marks it as disconnected and keeps the message pending.

## Known limitations

- There is no authentication. Anyone who knows a registered name can connect as that user; the protocol the assignment defines has no passwords.
- `GETFILE` will serve any file the owner's client process can read, by absolute path.
- The file-based storage with one global lock is simple rather than fast.
- `parte2` has two copies of the rpcgen output: `server/src/rpc` (client stubs used by the messaging server) and `rpc_server` (the actual service). The interface is the same in both; `rpc_server/log_rpc.x` is the commented one.
