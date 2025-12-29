#pragma once
#include <stdint.h>

int user_exec(const char* path);  // loads + enters ring3, never returns on success
