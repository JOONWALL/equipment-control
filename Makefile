CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Icommon/include -Iservices/equipmentd/include \
       -D_POSIX_C_SOURCE=200809L

SRC_COMMON = \
  common/src/protocol/line_codec.c \
  common/src/protocol/session.c

SRC_EQUIP = \
  services/equipmentd/src/main.c \
  services/equipmentd/src/router.c \
  services/equipmentd/src/connection_table.c

all: equipmentd

equipmentd: $(SRC_COMMON) $(SRC_EQUIP)
	$(CC) $(CFLAGS) -o equipmentd_bin $(SRC_COMMON) $(SRC_EQUIP)

clean:
	rm -f equipmentd_bin
