#include "yrc.h"
#include <stdio.h>
const char* msg = "try { var abba = {get x: 3, [y]: 3, n}; } catch(err) { }";

size_t readmsg(char* data, size_t desired) {
  static size_t idx = 0;
  static size_t cnt = 0;
  size_t i = 0;

  while(msg[idx] && i < desired) {
    data[i++] = msg[idx++];
  }

  if (!msg[idx] && cnt < 100) {
    ++cnt;
    idx = 0;
  }

  return i;
}

size_t readstdin(char* data, size_t desired) {
  if (feof(stdin)) {
    return 0;
  }
  return fread(data, 1, desired, stdin);
}

int main(int argc, const char** argv) {
  yrc_error_t* error;
  if (yrc_parse(readmsg, &error)) {
    printf("bad exit\n");
  }
  return 0;
}
