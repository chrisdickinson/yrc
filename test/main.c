#include "yrc.h"
#include <stdio.h>
#include <string.h>
const char* msg = "try { x++ * y; x--; var abba = {get x: 3, [y]: 3, n}; } catch(err) { }; function hey() {}";
FILE* inp;

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
  static int cnt = 0;
  if (feof(inp)) {
    if (cnt++ < 10) {
      fseek(inp, 0, SEEK_SET);
    } else {
      return 0;
    }
  }
  return fread(data, 1, desired, inp);
}

int main(int argc, const char** argv) {
  yrc_error_t* error;
  if (argc < 2) {
    printf("usage: run-tests [path-to-javascript]\n");
    return 1;
  }

  inp = fopen(argv[1], "r");
  if (yrc_parse(readstdin, &error)) {
    printf("bad exit\n");
  }
  fclose(inp);
  return 0;
}
