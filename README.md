# equipment-control

Linux(C)-based distributed equipment control architecture featuring
state-driven design and transport layer abstraction (Link / Session / Protocol).

Linux(C) 기반 중앙 제어 서버(equipmentd)가 TCP/Serial 장비 노드를 관리하는
분산 장비 제어 아키텍처 프로젝트

## 🎯 Project Goal

- 장비 제어 SW 구조를 고려한 분산 아키텍처 설계 및 구현
- 통신 드라이버 추상화(Link / Session / Protocol 분리)
- 상태 기반(State Machine) 제어 및 Timeout/Retry 처리
- 다중 장비 관리(DeviceManager) 구조 구현
- Test / Trouble Shooting 가능한 로그 및 재현 구조 설계

## 🧠 Architecture Overview

Control Server (equipmentd)
  ├── DeviceManager
  ├── StateMachine
  ├── Scheduler
  ├── CommandRouter
  └── Transport Layer
        ├── Link (fd I/O)
        ├── Session (reconnect, buffer)
        └── Protocol (Framer + Codec)

<img width="1039" height="584" alt="구조도" src="https://github.com/user-attachments/assets/512ea7c8-2afd-45c6-b77d-5176d59cbb72" />


## 🏗️ Design Principles

1. Core와 Transport 완전 분리
   - Core는 Message만 처리 (bytes 접근 금지)

2. Link / Session / Protocol 계층 분리
   - 통신 리팩토링 시 Core 영향 최소화

3. DeviceDriver 계층 도입
   - 장비별 프로토콜/에러 매핑 격리

4. Event-driven 상태 기반 제어
   - Command 나열이 아닌 StateMachine 기반 전이
     
5. Core boundary enforcement
   - DeviceManager / StateMachine never access raw bytes directly.

## 🚀 Roadmap

### Phase 1
- Single device TCP control
- Blocking I/O
- Basic logging

### Phase 2
- Non-blocking I/O (epoll)
- Timeout / Retry
- State Machine 도입
- Event-driven architecture stabilization

### Phase 3
- Multi-device management
- DeviceDriver abstraction

### Phase 4
- SerialChannel 분리
- Reconnect / Heartbeat

### Phase 5
- Test / Fault Injection
- Log replay

### Phase 6+
- systemd service integration
- mmap-based telemetry buffer
- (Optional) anomaly detection module
