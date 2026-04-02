#ifndef PMC_IO_MANAGER_H
#define PMC_IO_MANAGER_H

#include "pmc_connection.h"
#include "protocol/message.h"

typedef struct {
  int valid;
  int heater_known;
  int heater_on;
  int temp_known;
  float temp_c;
  char heater_raw[32];
  char temp_raw[32];
} pmc_aux_snapshot_t;

int pmc_io_send_message(pmc_connection_t* target, const message_t* msg);

int pmc_io_send_error(pmc_connection_t* target,
                      const message_t* req,
                      int code,
                      const char* text);

int pmc_io_send_status_cached(pmc_connection_t* eqd_conn,
                              const message_t* req,
                              const pmc_connection_t* sim_conn,
                              const pmc_aux_snapshot_t* aux);

int pmc_io_uart_ping(const char* serial_path,
                     int baudrate,
                     char* reply,
                     int reply_sz);

int pmc_io_uart_read_temp(const char* serial_path,
                          int baudrate,
                          char* reply,
                          int reply_sz);

int pmc_io_uart_set_heater(const char* serial_path,
                           int baudrate,
                           int on,
                           char* reply,
                           int reply_sz);

int pmc_io_uart_aux_configure(const char* serial_path, int baudrate);                           
void pmc_io_uart_aux_clear(void);
void pmc_io_uart_aux_shutdown(void);

int pmc_io_apply_aux_for_command(int dev_id,
                                 const message_t* msg,
                                 char* reply,
                                 int reply_sz);

int pmc_io_read_aux_snapshot(int dev_id, pmc_aux_snapshot_t* out);

#endif