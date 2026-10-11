#ifndef READER_M5PAPER_BUSES_H
#define READER_M5PAPER_BUSES_H

#include "driver/i2c_master.h"
#include "driver/spi_master.h"

#include <esp_err.h>

struct m5paper_buses {
    spi_device_handle_t display;
    i2c_master_bus_handle_t i2c;
};

esp_err_t m5paper_buses_init(struct m5paper_buses* buses);
void m5paper_buses_deinit(struct m5paper_buses* buses);

#endif
