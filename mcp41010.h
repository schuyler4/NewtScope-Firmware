#ifndef MCP41010_H
#define MCP41010_H

#define SET_WIPER_COMMAND 0b00010001

#define MAX_POT_CODE 255
#define MIN_POT_CODE 0

void write_pot_code(uint8_t *code);

#endif
