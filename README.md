# Equipment Control

## What
Linux/C 기반의 반도체 장비 제어 SW 구조를 모사하는 프로젝트입니다.  
상위 제어 서버(`equipmentd`), 중간 제어 노드(`PMC`), 장비 시뮬레이터(`sim`)를 분리해  
REQ/RESP/EVT 기반 통신과 상태 제어 구조를 구현하는 것을 목표로 합니다.

---

## Why
- 장비 제어 SW에 가까운 구조를 직접 설계/구현해보기 위해
- Linux system programming, TCP/IP, event-driven server(epoll) 역량을 프로젝트로 보여주기 위해
- 실제 장비 없이도 simulator 기반으로 상태 변화/telemetry를 검증하기 위해

---

## Current Architecture
text
equipmentd <-> PMC <-> sim

---

### equipmentd
상위 제어 서버
protocol parsing, routing, device state 관리 담당

### PMC
equipmentd 와 sim 사이의 중간 제어/중계 노드
메시지 전달 및 연결 관리 담당

### sim
가짜 장비 노드
FSM, process model, telemetry 생성 담당

---

## Progress

### Phase 1

- line-based text protocol 초안 작성

- session / line codec 기반 통신 골격 구현

Phase 2

- equipmentd epoll 기반 event loop 구현

- TCP connection / session / framing / parsing 동작 검증

- fake simulator 와 equipmentd 간 기본 통신 확인

- Timeout 구현 및 multidevice 대응

Phase 3

- device_manager 도입

- START / STOP / RESET / FAULT / STATUS 처리 추가

- simulator 측 FSM / process model / telemetry 구조 분리

Current

- equipmentd <-> sim 직접 통신 검증 완료

- PMC 도입 시작

- 다음 목표: equipmentd <-> PMC <-> sim 중계 경로 완성

---

## Directory Overview
common/
  include/protocol/   # message, session, codec
  src/protocol/

services/
  equipmentd/         # upper control server

nodes/
  deviced/sim/        # device simulator
  pmc/                # process module controller (in progress)

docs/
  architecture.md
  protocol/spec.md

---

## Next Steps

- PMC 최소 골격 구현

- equipmentd <-> PMC <-> sim 메시지 중계 확인

- equipmentd 구조 정리(router / device_manager / connection)

- protocol detail 정리 및 문서 업데이트

- 이후 heartbeat / reconnect 검토

---

## Notes

현재 README는 상세 구현 문서가 아니라 진행 상황 리마인드용으로 유지

세부 설계는 docs/architecture.md, docs/protocol/spec.md에서 관리
---
