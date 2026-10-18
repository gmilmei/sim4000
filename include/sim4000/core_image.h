#ifndef SIM4000_CORE_IMAGE_H
#define SIM4000_CORE_IMAGE_H

#include <stddef.h>

#include "sim4000/machine.h"

int rc_core_image_load(const char *path, rc_machine_t *machine,
                       rc_addr_t start_address, size_t *word_count,
                       char *error, size_t error_size);
int rc_core_image_save(const char *path, const rc_machine_t *machine,
                       rc_addr_t start_address, size_t word_count,
                       char *error, size_t error_size);

#endif
