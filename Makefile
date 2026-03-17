CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Icommon/include -Iservices/equipmentd/include \
       -Inodes/deviced/sim/include \
       -D_POSIX_C_SOURCE=200809L

SRC_COMMON = \
  common/src/protocol/line_codec.c \
  common/src/protocol/session.c

SRC_EQUIP = \
  services/equipmentd/src/main.c \
  services/equipmentd/src/router.c \
  services/equipmentd/src/connection_table.c\
  services/equipmentd/src/device_manager.c

SRC_SIM = \
  nodes/deviced/sim/src/main.c \
  nodes/deviced/sim/src/fsm.c \
  nodes/deviced/sim/src/process_model.c \
  nodes/deviced/sim/src/telemetry.c

all: equipmentd sim

equipmentd:
	$(CC) $(CFLAGS) -o equipmentd_bin $(SRC_COMMON) $(SRC_EQUIP)

sim:
	$(CC) $(CFLAGS) -o device_sim_fake $(SRC_COMMON) $(SRC_SIM)

clean:
	rm -f equipmentd_bin device_sim_fake
