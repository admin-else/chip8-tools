#include <stdarg.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  char *data;
  size_t len;
  size_t cap;
  size_t pos;
} data_buffer;

void data_buffer_free(data_buffer *buff) {
  free(buff->data);
  buff->len = 0;
  buff->pos = 0;
  buff->cap = 0;
}

data_buffer *data_buffer_init_w_size(size_t size) {
  data_buffer *buff = malloc(sizeof(data_buffer));
  buff->data = malloc(size);
  buff->len = size;
  buff->cap = size;
  buff->pos = 0;
  return buff;
}

data_buffer *data_buffer_init() {
  data_buffer *buff = malloc(sizeof(data_buffer));
  buff->data = NULL;
  buff->len = 0;
  buff->cap = 0;
  buff->pos = 0;
  return buff;
}

int data_buffer_push(data_buffer *buff, const void *data, size_t data_size) {
  if (data_size == 0)
    return 0;
  if (buff == NULL || data == NULL)
    return 1;

  if (buff->data == NULL) {
    buff->data = malloc(data_size);
    buff->cap = data_size;
    buff->len = data_size;
    return 0;
  }

  return 0;
}

data_buffer *complile_line_by_line(data_buffer *buff) {
  data_buffer *out = data_buffer_init();
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("usage: chip8e your_code.txt\n");
    return 1;
  }

  // read file
  FILE *input_f = fopen(argv[1], "r");
  if (input_f == NULL) {
    perror("Error opening file");
    exit(1);
  }
  fseek(input_f, 0, SEEK_END);
  data_buffer *buffer = data_buffer_init_w_size(ftell(input_f));
  rewind(input_f);
  fread(buffer->data, 1, buffer->len, input_f);
  buffer->data[buffer->len] = '\0';
  fclose(input_f);

  data_buffer_free(buffer);
  return 0;
}