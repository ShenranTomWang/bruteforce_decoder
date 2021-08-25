#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <crypt.h>

//EFFECTS: print encrypted hash code for given password with given salt
int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s PASSWORD SALT", argv[0]);
  }
  struct crypt_data data;
  printf("%s\n", crypt_r(argv[1], argv[2], &data));
  return 0;
}
