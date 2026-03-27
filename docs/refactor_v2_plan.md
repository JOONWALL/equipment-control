# Refactor Plan: Cluster Tool V2

## Why
- Current structure: equipmentd ↔ pmc ↔ sim
- Limitations:
  - no scheduler
  - no TMC
  - PMC acts as relay
  - weak equipment-control structure

## Goal
- CTC / TMC / PMC architecture
- state-based scheduler with retry
- PMC internal split: io / sequence / alarm
- UART device integration
- host (GEM-like) mock

## Target Architecture
- Pi5: CTC
- Pi4: TMC
- Pi3: PMC node
  - pmc_preclean
  - pmc_deposition
- Pico: UART device

## Strategy
- keep common/protocol/session assets
- evolve equipmentd into CTC
- absorb sim logic into PMC internals
- remove current PMC relay role
- add TMC and hostsim
