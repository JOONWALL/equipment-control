# Protocol Spec (Phase 1)

## Transport
- TCP
- Text line framing: each message ends with '\n'
- Encoding: UTF-8 ASCII subset recommended

## Message Format
A single line consists of:
<ROLE> <TYPE> <KV...>\n

- ROLE: REQ | RESP | EVT
- TYPE: PING | START | STOP | STATUS | ERROR
- KV: key=value pairs separated by space

### Common Keys
- dev=<int>      device id (required for device-specific actions)
- seq=<int>      request sequence id (optional in Phase1, recommended)
- code=<int>     error code
- msg=<string>   human readable message (no spaces, use '_' instead)

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
