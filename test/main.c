#include "yrc.h"
#include <stdio.h>
const char* msg = "if (/asdf/) b\n else c";

size_t readmsg(char* data, size_t desired) {
  static size_t idx = 0;
  size_t i = 0;

  while(msg[idx] && i < desired) {
    data[i++] = msg[idx++];
  }

  printf("read %lu\n", i);
  return i;
}

size_t readstdin(char* data, size_t desired) {
  if (feof(stdin)) {
    return 0;
  }
  return fread(data, 1, desired, stdin);
}

int main(int argc, const char** argv) {
  if (yrc_parse(readmsg)) {
    printf("bad exit\n");
  }
  return 0;
}
