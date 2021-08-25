#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>

#include "uthread.h"
#include "threadpool.h"

//Max password lenth set to 5
#define MAX_PSW_LEN 5

char* cheat[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
char* str;
int psw_found = 0;

//EFFECTS: returns 1 if hash_str is equal to str
int str_equals(char* hash_str, char* str) {
  if (strlen(hash_str) != strlen(str)) {
    return 0;
  } else {
    for (int i = 0; i < strlen(hash_str); i++) {
      if (hash_str[i] != str[i]) {
        return 0;
      }
    }
    return 1;
  }
}

//REQUIRES: pool and arg != null
//MODIFIES: this
//EFFECTS: if password is not decrypted, find it, and if current attempt is not successful and digit < MAX_PSW_LEN, add one digit, otherwise
//         stop the process
void try(tpool_t pool, void *arg) {
  if (!psw_found) {
    char* i = arg;
    struct crypt_data cd = {};
    char* hash_str = crypt_r(i, str, &cd);
    if (str_equals(hash_str, str)) {
      printf("the password is %s\n", i);
      psw_found = 1;
      return;
    } else if (strlen(i) >= MAX_PSW_LEN) {
      return;
    } else {
      for (int j = 0; j < 10; j++) {
        char* s = malloc(sizeof(char)*(strlen(i) + 2));
        strcpy(s, i);
        strcat(s, cheat[j]);
        tpool_schedule_task(pool, try, (void*)s);
      }
    }
  }
}

//MODIFIES: this
//EFFECTS: main function
int main(int argc, char *argv[]) {

  tpool_t pool;
  int num_threads;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s NUM_THREADS HASH_CODE\n", argv[0]);
    return -1;
  }
  
  num_threads = strtol(argv[1], NULL, 10);
  str = argv[2];
  
  uthread_init(8);
  pool = tpool_create(num_threads);

  for (int i = 0; i < 10; i++) {
    tpool_schedule_task(pool, try, (void*)cheat[i]);
  }

  tpool_join(pool);
  
  return 0;
}
