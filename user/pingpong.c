#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
  int p1[2];
  int p2[2];

  pipe(p1);
  pipe(p2);

  int pid = fork();

  if (pid < 0){
    fprintf(2, "fork failed\n");
    exit(1);
  }
  if (pid == 0){
    close(p1[1]);
    close(p2[0]);

    char buf[1];
    for (int i = 0; i < 10; i++){
      read(p1[0], buf, 1);
      printf("Child received ball %d\n", i+1);
      write(p2[1], "x", 1);
    }

    close(p1[0]);
    close(p2[1]);
    exit(0);
  }
  else {
    close(p1[0]);
    close(p2[1]);

    char buf[1];
    for (int i = 0; i < 10; i++){
      write(p1[1], "x", 1);
      read(p2[0], buf, 1);
      printf("Parent received ball %d\n", i+1);
    }

    close(p1[0]);
    close(p2[1]);
    wait(0);
  }
  exit(0);
}
