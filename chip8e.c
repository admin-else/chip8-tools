#include <bits/types/stack_t.h>
#include <raylib.h>

#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARRAY_SIZE(EXPR) (sizeof(EXPR) / sizeof((EXPR)[0]))
#define SET_DISPLAY(x, y, val) state->display[y * 64 + x] = val;

enum { window_width = 640, window_height = 320, window_pixel_size = 10 };

enum { CHIP8_MEMORY_SIZE = 4 * 1024 }; // 4kb memory
enum {
  CHIP8_DISPLAY_COLUMNS = 64,
  CHIP8_DISPLAY_ROWS = 32,
  CHIP8_DISPLAY_MAX_PIXELID = CHIP8_DISPLAY_COLUMNS * CHIP8_DISPLAY_ROWS
};

const unsigned char fontset[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct chip8state {
  uint8_t memory[CHIP8_MEMORY_SIZE];
  uint8_t V[16]; // Registers

  uint16_t stack[16]; // wikipedia says 12 slots but 16 is cooler

  uint16_t I;  // addr register
  uint16_t PC; // Pgram counter
  uint8_t SP;  // stack pointer
  uint8_t ST;  // sound timer
  uint8_t DT;  // delay timer

  // display can only have states on / off
  uint8_t display[CHIP8_DISPLAY_COLUMNS * CHIP8_DISPLAY_ROWS];
  uint16_t keyboard;
  int8_t reg_awaing_key; // -1 means not awating
};

void draw_display(uint8_t *display) {
  ClearBackground((Color){0, 0, 0, 255});
  for (int x = 0; x < CHIP8_DISPLAY_COLUMNS; x++) {
    for (int y = 0; y < CHIP8_DISPLAY_ROWS; y++) {
      if (display[y * CHIP8_DISPLAY_COLUMNS + x])
        DrawRectangle(x * window_pixel_size, y * window_pixel_size,
                      window_pixel_size, window_pixel_size,
                      (Color){255, 255, 255, 255});
    }
  }
}

static uint16_t next_instruction(struct chip8state *state) {
  assert(state->PC < ARRAY_SIZE(state->memory));
  uint16_t inst = be16toh(*(uint16_t *)&state->memory[state->PC]);
  state->PC += sizeof(inst);
  return inst;
}

// helpers
uint8_t static inline inst_type(uint16_t inst) {
  return inst & 0xF000 >> 3 * 4;
}

uint8_t static inline x(uint16_t inst) { return inst & 0x0F00 >> 2 * 4; }
uint8_t static inline y(uint16_t inst) { return inst & 0x00F0 >> 1 * 4; }
uint8_t static inline nnn(uint16_t inst) { return inst & 0x0FFF >> 0 * 4; }
uint8_t static inline nn(uint16_t inst) { return inst & 0x00FF >> 0 * 4; }
uint8_t static inline n(uint16_t inst) { return inst & 0x000F >> 0 * 4; }

void simulate_instruction(struct chip8state *state) {
  uint16_t inst = next_instruction(state);
  switch (inst_type(inst)) {
  case 0:
    switch (nn(inst)) {
    case 0xE0: // 00E0
      memset(state->display, 0, sizeof(state->display));
      return;
    case 0xEE: // 00EE
      state->PC = state->stack[state->SP--];
      return;
    default:  // 0NNN
      return; // unsuported 0NNN treat as nop
    }
  case 1: // 1NNN
    state->PC = nnn(inst);
    return;
  case 2: // 2NNN
    state->stack[state->SP++] = state->PC;
    state->PC = nnn(inst);
    return;
  case 3: // 3XNN
    if (state->V[x(inst)] == nn(inst))
      state->PC++;
    return;
  case 4: // 4XNN
    if (state->V[x(inst)] != nn(inst))
      state->PC++;
    return;
  case 5: // 5XY0
    if (state->V[x(inst)] != state->V[y(inst)])
      state->PC++;
    return;
  case 6: // 6XNN
    state->V[x(inst)] = nn(inst);
    return;
  case 7: // 7XNN
    state->V[x(inst)] += nn(inst);
    return;
  case 8: // these are very easy
    switch (n(inst)) {
    case 0: // 8XY0
      state->V[x(inst)] = state->V[y(inst)];
      return;
    case 1: // 8XY1
      state->V[x(inst)] |= state->V[y(inst)];
      return;
    case 2: // 8XY2
      state->V[x(inst)] &= state->V[y(inst)];
      return;
    case 3: // 8XY3
      state->V[x(inst)] ^= state->V[y(inst)];
      return;
      // now the harder ones
      // btw the old value are for the case that the flag register gets used
    case 4: // 8XY4
    {
      uint16_t res = state->V[x(inst)] + state->V[y(inst)];
      state->V[x(inst)] = res & 0xFF;
      state->V[0xF] = res > 0xFF;
      return;
    }
    case 5: // 8XY5
    {
      uint8_t oldx = state->V[x(inst)];
      state->V[x(inst)] -= state->V[y(inst)];
      state->V[0xF] = oldx < state->V[y(inst)];
      return;
    }
    case 6: // 8XY6
    {
      uint8_t oldx = state->V[x(inst)];
      state->V[x(inst)] >>= state->V[y(inst)];
      state->V[0xF] = oldx & 1;
      return;
    }
    case 7: // 8XY7
    {
      state->V[x(inst)] = state->V[y(inst)] - state->V[x(inst)];
      state->V[0xF] = state->V[x(inst)] < state->V[y(inst)];
      return;
    }
    case 0xE: // 8XYE
    {
      uint8_t oldx = state->V[x(inst)];
      state->V[x(inst)] <<= 1;
      state->V[0xF] = (oldx >> 7) & 0x1;
      return;
    }
    }
  case 9: // 9XY0
    if (state->V[x(inst)] != state->V[y(inst)])
      state->PC++;
    return;
  case 0xA: // ANNN
    state->I = nnn(inst);
    return;
  case 0xB: // BNNN
    state->PC = nnn(inst) + state->V[0];
    return;
  case 0xC: // CXNN
    state->V[x(inst)] = rand() & nn(inst);
    return;
  case 0xD: { // DXYN
    uint8_t x_off = state->V[x(inst)], y_off = state->V[y(inst)], flag = false;
    for (int y = 0; y < n(inst); y++) {
      for (int x = 0; x < 8; x++) {
        uint16_t pixel_id = (y_off + y) * CHIP8_DISPLAY_COLUMNS + x_off + x;
        if (pixel_id > CHIP8_DISPLAY_MAX_PIXELID)
          continue; // outside of screen
        bool val = state->memory[state->I + y] & 1 << x;
        if (state->display[pixel_id] && val)
          flag = true;
        state->display[pixel_id] ^= val;
      }
    }
    state->V[0xF] = flag;
    return;
  }
  case 0xE: // IO instructions
    switch (nn(inst)) {
    case 0x9E: // EX9E
      if (state->keyboard & (1 << state->V[x(inst)]))
        state->PC++;
      return;
    case 0x1A: // EX1A
      if (!(state->keyboard & (1 << state->V[x(inst)])))
        state->PC++;
      return;
    }
  case 0xF:
    switch (nn(inst)) {
    case 0x07: // FX07
      state->V[x(inst)] = state->DT;
      return;
    case 0x0A: // FX0A
      state->reg_awaing_key = x(inst);
      return;
    case 0x15: // FX15
      state->DT = state->V[x(inst)];
      return;
    case 0x18: // FX18
      state->ST = state->V[x(inst)];
      return;
    case 0x1E: // FX1E
      state->I += state->V[x(inst)];
      return;
    case 0x29: // FX29
      state->I = state->V[x(inst)] * 5;
      return;
    case 0x33: // FX33
    {
      uint8_t val = state->V[x(inst)];
      for (char i = 3; i > 0; i--) {
        state->memory[state->I + i] = val % 10;
        val /= 10;
      }
      return;
    }
    case 0x55: // FX55
      memcpy(state->memory + state->I, state->V, x(inst) + 1);
      return;
    case 0x65: // FX65
      memcpy(state->V, state->memory + state->I, x(inst) + 1);
      return;
    }
  }
}

int main(int argc, char **argv) {
  srand(0);
  // srand(time(NULL));
  uint8_t display[CHIP8_DISPLAY_COLUMNS * CHIP8_DISPLAY_ROWS];
  memset(display, 0, ARRAY_SIZE(display));

  InitWindow(window_width, window_height, "chip8e");

  struct chip8state *state = malloc(sizeof(struct chip8state));

  // load rom
  FILE *rom_file = fopen("roms/tetris.ch8", "rb");
  fseek(rom_file, 0, SEEK_END);
  ssize_t rom_size = ftell(rom_file);
  rewind(rom_file);
  fread(state->memory + 0x200, 1, rom_size, rom_file);

  // setup registers

  while (!WindowShouldClose()) {
    BeginDrawing();

    draw_display(display);
    display[0] = !display[0];
    EndDrawing();
  }

  CloseWindow();
  return 0;
}