#ifndef SOURCE_RANDOM_TOOL_H_
#define SOURCE_RANDOM_TOOL_H_

#include "cyhal.h"
#include "cybsp.h"  
#include <stdio.h>

#define ASCII_7BIT_MASK                 (0x7F)

#define PASSWORD_LENGTH                 (8u)
#define ASCII_VISIBLE_CHARACTER_START   (33u)
#define ASCII_RETURN_CARRIAGE           (0x0D)


/* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
#define CLEAR_SCREEN         "\x1b[2J\x1b[;H"

extern uint8_t password[PASSWORD_LENGTH + 1];

void generate_password();
uint8_t check_range(uint8_t value);


#endif