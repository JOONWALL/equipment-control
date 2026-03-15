# Protocol Spec

## Transport
- TCP
- Text line framing: each message ends with '\n'
- Encoding: UTF-8 ASCII subset recommended

## Message Format
A single line consists of:
<ROLE> <TYPE> <KV...>\n

- ROLE: REQ | RESP | EVT
- TYPE: PING | START | STOP | STATUS | RESET | FAULT | ERROR
- KV: key=value pairs separated by space

### Common Keys
- dev=<int>      device id (required for device-specific actions)
- seq=<int>      request sequence id (optional in Phase1, recommended)
- code=<int>     error code
- msg=<string>   human readable message (no spaces, use '_' instead)
- state=<token>  device state (e.g. IDLE, RUNNING, ERROR)
- ok=<0|1>       response result

## Examples

### Ping
REQ PING seq=1
RESP PING seq=1 ok=1

### Start/Stop
REQ START dev=1 seq=10
RESP START dev=1 seq=10 ok=1

REQ STOP dev=1 seq=11
RESP STOP dev=1 seq=11 ok=1

### Status (device -> server or server -> device)
REQ STATUS dev=1 seq=20
RESP STATUS dev=1 seq=20 ok=1 state=IDLE

EVT STATUS dev=1 state=RUNNING

### Error
RESP ERROR dev=1 seq=10 ok=0 code=1001 msg=TIMEOUT
EVT  ERROR dev=1 code=2002 msg=OVERCURRENT

## Rules
- Unknown keys must be ignored (forward compatibility).
- Core modules must only see parsed Message objects (no raw bytes).

## Device State Machine

- IDLE -> START -> RUNNING
- RUNNING -> STOP -> IDLE
- IDLE/RUNNING -> FAULT -> ERROR
- ERROR -> RESET -> IDLE

### Fault / Reset

REQ FAULT dev=1 seq=30
RESP FAULT dev=1 seq=30 ok=1

REQ STATUS dev=1 seq=31
RESP STATUS dev=1 seq=31 ok=1 state=ERROR

REQ RESET dev=1 seq=32
RESP RESET dev=1 seq=32 ok=1

REQ STATUS dev=1 seq=33
RESP STATUS dev=1 seq=33 ok=1 state=IDLE

REQ RESET dev=1 seq=34
RESP ERROR dev=1 seq=34 ok=0 code=409 msg=RESET_REQUIRES_ERROR_STATE