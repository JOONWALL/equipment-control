CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Icommon/include -Iservices/equipmentd/include \
       -Inodes/deviced/sim/include \
       -Inodes/pmc/include \
       -D_POSIX_C_SOURCE=200809L

SRC_COMMON = \
  common/src/protocol/line_codec.c \
  common/src/protocol/session.c

SRC_EQUIP = \
  services/equipmentd/src/main.c \
  services/equipmentd/src/router.c \
  services/equipmentd/src/connection_table.c \
  services/equipmentd/src/device_manager.c \
  services/equipmentd/src/module_registry.c

SRC_PMC = \
  nodes/pmc/src/main.c \
  nodes/pmc/src/pmc_connection.c \
  nodes/pmc/src/pmc_router.c

SRC_SIM = \
  nodes/deviced/sim/src/main.c \
  nodes/deviced/sim/src/fsm.c \
  nodes/deviced/sim/src/process_model.c \
  nodes/deviced/sim/src/telemetry.c

SRC_TMC = \
  nodes/tmc/src/main.c

SRC_HOSTSIM = \
  services/hostsim/src/main.c

all: equipmentd pmc sim tmc hostsim

equipmentd:
	$(CC) $(CFLAGS) -o equipmentd_bin $(SRC_COMMON) $(SRC_EQUIP)

pmc:
	$(CC) $(CFLAGS) -o pmc_bin $(SRC_COMMON) $(SRC_PMC)

sim:
	$(CC) $(CFLAGS) -o device_sim_fake $(SRC_COMMON) $(SRC_SIM)

clean:
	rm -f equipmentd_bin pmc_bin device_sim_fake

tmc:
	$(CC) $(CFLAGS) -o tmc_bin $(SRC_COMMON) $(SRC_TMC)

hostsim:
	$(CC) $(CFLAGS) -o hostsim_bin $(SRC_COMMON) $(SRC_HOSTSIM)