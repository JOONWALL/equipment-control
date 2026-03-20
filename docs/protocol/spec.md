# Protocol Spec

## Transport
- TCP
- Text line framing: each message ends with '\n'
- Encoding: UTF-8 ASCII subset recommended

## Message Format
A single line consists of:
<ROLE> <TYPE> <KV...>\n

- ROLE: REQ | RESP | EVT
- TYPE: PING | REGISTER | START | STOP | STATUS | RESET | FAULT | ERROR | TELEMETRY | ALARM
- KV: key=value pairs separated by space

### Common Keys
- dev=<int>      device id (required for device-specific actions)
- seq=<int>      request sequence id
- code=<int>     error code
- msg=<string>   human readable message (no spaces, use '_' instead)
- state=<token>  device state (e.g. IDLE, RUNNING, ERROR)
- ok=<0|1>       response result

## Register

PMC 도입 이후 peer 식별을 위해 연결 직후 REGISTER 메시지를 사용한다.

### equipmentd -> PMC
REQ REGISTER seq=1

### sim -> PMC
EVT REGISTER dev=1

## Examples

### Ping
REQ PING seq=1
RESP PING seq=1 ok=1

### Start/Stop
REQ START dev=1 seq=10
RESP START dev=1 seq=10 ok=1

REQ STOP dev=1 seq=11
RESP STOP dev=1 seq=11 ok=1

### Status
REQ STATUS dev=1 seq=20
RESP STATUS dev=1 seq=20 ok=1 state=IDLE

EVT STATUS dev=1 state=RUNNING

### Telemetry
EVT TELEMETRY dev=1 temp=25.00 pressure=1.00 flow=0.00

### Error
RESP ERROR dev=1 seq=10 ok=0 code=1001 msg=TIMEOUT
EVT ERROR dev=1 code=2002 msg=OVERCURRENT

## Rules
- Unknown keys must be ignored (forward compatibility).
- Core modules must only see parsed Message objects (no raw bytes).

## Device State Machine
- IDLE -> START -> RUNNING
- RUNNING -> STOP -> IDLE
- IDLE/RUNNING -> FAULT -> ERROR
- ERROR -> RESET -> IDLE