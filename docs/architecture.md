# Architecture

- Control Server: services/equipmentd (Pi5)
- Device Nodes: nodes/deviced-pi3, nodes/deviced-sim
- Shared contract: protocol/spec.md + common/include/protocol

Core principles:
- Core handles Message/Event only (no raw bytes).
- Transport split: Link / Session / Protocol (Framer + Codec)
- Device-specific behavior isolated in DeviceDriver.

# Notes

Pi5 = CTC
Pi4 = TMC
Pi3 = PMC node
Pico = UART device
single wafer
preclean + deposition
scheduler + retry
hostsim