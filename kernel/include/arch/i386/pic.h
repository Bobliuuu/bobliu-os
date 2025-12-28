#pragma once
#include <stdint.h>

void pic_remap(int offset1, int offset2);
void pic_clear_mask(uint8_t irq_line);
void pic_set_mask(uint8_t irq_line);