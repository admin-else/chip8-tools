#include <raylib.h>

#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
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
  int8_t reg_awating_key; // -1 means not awating
  bool jumped;
};

void print_time() {
  time_t t = time(NULL);
  struct tm ttm = *localtime(&t);
  printf("[%02d:%02d:%02d] ", ttm.tm_hour, ttm.tm_min, ttm.tm_sec);
}

#define LOG_DEBUG(...) printf(__VA_ARGS__)

#define LOG_DEBUG_W_TIME(...)                                                  \
  print_time();                                                                \
  printf(__VA_ARGS__)
// #define LOG_DEBUG(...)

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

// helpers
uint8_t static inline inst_type(uint16_t inst) {
  return (inst & 0xF000) >> 3 * 4;
}

void simulate_instruction(struct chip8state *state) {
#define JUMP(addr)                                                             \
  do {                                                                         \
    state->PC = addr;                                                          \
    state->jumped = true;                                                      \
  } while (0)
#define X ((inst & 0x0F00) >> 2 * 4)
#define Y ((inst & 0x00F0) >> 1 * 4)
#define NNN (inst & 0x0FFF)
#define NN (inst & 0x00FF)
#define N (inst & 0x000F)
#define VX state->V[X]
#define VY state->V[Y]
#define V0 state->V[0]
#define VF state->V[0xF]

  assert(state->PC < ARRAY_SIZE(state->memory));
  uint16_t inst = be16toh(*(uint16_t *)&state->memory[state->PC]);
  if (!state->jumped)
    state->PC += sizeof(inst);
  state->jumped = false;
  LOG_DEBUG_W_TIME("0x%04X: %04X ", state->PC, inst);
  switch (inst_type(inst)) {
  case 0:
    switch (NN) {
    case 0xE0: // 00E0
      LOG_DEBUG("clear screen\n");
      memset(state->display, 0, sizeof(state->display));
      return;
    case 0xEE: // 00EE
      state->SP--;
      JUMP(state->stack[state->SP]);
      LOG_DEBUG("return to %03X\n", state->PC);
      return;
    default: // 0NNN
      LOG_DEBUG("unsuported %X\n", NNN);
      return; // unsuported 0NNN treat as nop
    }
  case 1: // 1NNN
    JUMP(NNN);
    LOG_DEBUG("jump %X\n", state->PC + 2);
    return;
  case 2: // 2NNN
    state->stack[state->SP] = state->PC;
    state->SP++;
    JUMP(NNN);
    LOG_DEBUG("call %03X\n", state->PC);
    return;
  case 3: // 3XNN
    if (VX == NN)
      state->PC += sizeof(inst);
    LOG_DEBUG("if V%X == %X\n", X, NN);
    return;
  case 4: // 4XNN
    if (VX != NN)
      state->PC += sizeof(inst);
    LOG_DEBUG("if V%X != %X\n", X, NN);
    return;
  case 5: // 5XY0
    if (VX != VY)
      state->PC += sizeof(inst);
    LOG_DEBUG("if V%X != V%X\n", X, Y);
    return;
  case 6: // 6XNN
    VX = NN;
    LOG_DEBUG("V%X = 0x%X\n", X, NN);
    return;
  case 7: // 7XNN
    VX += NN;
    LOG_DEBUG("V%X += 0x%X\n", X, NN);
    return;
  case 8: // these are very easy
    switch (N) {
    case 0: // 8XY0
      VX = VY;
      LOG_DEBUG("V%X = V%X\n", X, Y);
      return;
    case 1: // 8XY1
      VX |= VY;
      LOG_DEBUG("V%X |= V%X\n", X, Y);
      return;
    case 2: // 8XY2
      VX &= VY;
      LOG_DEBUG("V%X &= V%X\n", X, Y);
      return;
    case 3: // 8XY3
      VX ^= VY;
      LOG_DEBUG("V%X ^= V%X\n", X, Y);
      return;
      // now the harder ones
      // btw the old value are for the case that the flag register gets used
    case 4: // 8XY4
    {
      uint16_t res = VX + VY;
      VX = res & 0xFF;
      VF = res > 0xFF;
      LOG_DEBUG("V%X += V%X // with carry\n", X, Y);
      return;
    }
    case 5: // 8XY5
    {
      uint8_t oldx = VX;
      VX -= VY;
      VF = oldx < VY;
      LOG_DEBUG("V%X -= V%X // with carry\n", X, Y);
      return;
    }
    case 6: // 8XY6
    {
      uint8_t oldx = VX;
      VX >>= VY;
      VF = oldx & 1;
      LOG_DEBUG("V%X >>= V%X // with carry\n", X, Y);
      return;
    }
    case 7: // 8XY7
    {
      VX = VY - VX;
      VF = VX < VY;
      LOG_DEBUG("V%X = V%X - V%X // with carry\n", X, Y, X);
      return;
    }
    case 0xE: // 8XYE
    {
      uint8_t oldx = VX;
      VX <<= 1;
      VF = (oldx >> 7) & 0x1;
      LOG_DEBUG("V%X <<= V%X // with carry\n", X, Y);
      return;
    }
    }
  case 9: // 9XY0
    if (VX != VY)
      state->PC += sizeof(inst);
    LOG_DEBUG("if V%X != V%X\n", X, Y);
    return;
  case 0xA: // ANNN
    state->I = NNN;
    LOG_DEBUG("I = %X\n", NNN);
    return;
  case 0xB: // BNNN
    JUMP(NNN + state->V[0]);
    LOG_DEBUG("I = %X\n + V0", NNN);
    return;
  case 0xC: // CXNN
    VX = rand() & NN;
    LOG_DEBUG("V%X = %X & rand()\n", X, NN);
    return;
  case 0xD: { // DXYN
    VF = false;
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < 8; x++) {
        uint16_t pixel_id =
            ((VY + y) % CHIP8_DISPLAY_ROWS) * CHIP8_DISPLAY_COLUMNS +
            (VX + x) % CHIP8_DISPLAY_COLUMNS;
        bool val = state->memory[state->I + y] & 1 << x;
        if (state->display[pixel_id] && val)
          VF = true;
        state->display[pixel_id] ^= val;
      }
    }
    LOG_DEBUG(
        "sprite(V%X, V%X, %X) real displays at off_x=%d off_y=%d x=8 y=%d\n", X,
        Y, N, VX, VY, N);
    return;
  }
  case 0xE: // IO instructions
    switch (NN) {
    case 0x9E: // EX9E
      if (state->keyboard & (1 << VX))
        state->PC += sizeof(inst);
      LOG_DEBUG("if key == V%X\n", X);
      return;
    case 0x1A: // EX1A
      if (!(state->keyboard & (1 << VX)))
        state->PC += sizeof(inst);
      LOG_DEBUG("if key != V%X\n", X);
      return;
    }
  case 0xF:
    switch (NN) {
    case 0x07: // FX07
      VX = state->DT;
      LOG_DEBUG("V%X = delay_timer\n", X);
      return;
    case 0x0A: // FX0A
      state->reg_awating_key = X;
      LOG_DEBUG("V%X = key\n", X);
      return;
    case 0x15: // FX15
      state->DT = VX;
      LOG_DEBUG("delay_timer = V%X\n", X);
      return;
    case 0x18: // FX18
      state->ST = VX;
      LOG_DEBUG("sound_timer = V%X\n", X);
      return;
    case 0x1E: // FX1E
      state->I += VX;
      LOG_DEBUG("I += V%X\n", X);
      return;
    case 0x29: // FX29
      state->I = VX * 5;
      LOG_DEBUG("I = loc_of_font(V%X)\n", X);
      return;
    case 0x33: // FX33
    {
      uint8_t val = VX;
      for (char i = 3; i > 0; i--) {
        state->memory[state->I + i] = val % 10;
        val /= 10;
      }
      LOG_DEBUG("I = BCD(V%X)\n", X);
      return;
    }
    case 0x55: // FX55
      memcpy(state->memory + state->I, state->V, X + 1);
      LOG_DEBUG("put registers till V%X\n", X);
      return;
    case 0x65: // FX65
      memcpy(state->V, state->memory + state->I, X + 1);
      LOG_DEBUG("load registers till V%X\n", X);
      return;
    }
  }

#undef JUMP
#undef X
#undef Y
#undef NNN
#undef NN
#undef N
#undef VX
#undef VY
#undef V0
#undef VF
}

uint16_t get_keyboard() {
  uint16_t keyboard = 0;
#define MAP_KEY(key_id, key) keyboard |= IsKeyPressed(key) << key_id;
  MAP_KEY(1, KEY_ONE);
  MAP_KEY(2, KEY_TWO);
  MAP_KEY(3, KEY_THREE);
  MAP_KEY(0xC, KEY_FOUR);

  // row 2
  MAP_KEY(4, KEY_Q);
  MAP_KEY(5, KEY_W);
  MAP_KEY(6, KEY_E);
  MAP_KEY(0xD, KEY_R);

  // row 3
  MAP_KEY(7, KEY_A);
  MAP_KEY(8, KEY_S);
  MAP_KEY(9, KEY_D);
  MAP_KEY(0xE, KEY_F);

  // row 4
  MAP_KEY(
      0xA,
      KEY_Y); // you may change this to KEY_Z if you have a american keyboard
  MAP_KEY(0, KEY_X);
  MAP_KEY(0xB, KEY_C);
  MAP_KEY(0xF, KEY_V);
#undef MAP_KEY
  return keyboard;
}

int main(int argc, char **argv) {
  srand(0);
  // srand(time(NULL));
  uint8_t display[CHIP8_DISPLAY_COLUMNS * CHIP8_DISPLAY_ROWS];
  memset(display, 0, ARRAY_SIZE(display));

  InitWindow(window_width, window_height, "chip8e");

  struct chip8state *state = malloc(sizeof(struct chip8state));
  memset(state, 0, sizeof(struct chip8state));

  // load rom
  FILE *rom_file = fopen("output.ch8", "rb");
  fseek(rom_file, 0, SEEK_END);
  ssize_t rom_size = ftell(rom_file);
  rewind(rom_file);
  fread(state->memory + 0x200, 1, rom_size, rom_file);

  // load font
  memcpy(state->memory, fontset, ARRAY_SIZE(fontset));

  // setup registers
  state->jumped = true;
  state->PC = 0x200; // one before bec the simulate instruction increments
                     // before executing
  state->reg_awating_key = -1;

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    state->keyboard = get_keyboard();
    if (state->reg_awating_key != -1) {
      for (int key_id = 0; key_id < 0xF; key_id++) {
        if (state->keyboard & (1 << key_id)) {
          LOG_DEBUG_W_TIME("got awaited key %i\n", key_id);
          state->V[state->reg_awating_key] = key_id;
          state->reg_awating_key = -1;
          break;
        }
      }
    } else
      simulate_instruction(state);

    BeginDrawing();
    draw_display(state->display);
    EndDrawing();
    // getchar();
  }

  CloseWindow();
  return 0;
}
