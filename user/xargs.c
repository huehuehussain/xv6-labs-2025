#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXARGS 32

int
main(int argc, char *argv[])
{
  char *args[MAXARGS];
  int n;

  if(argc < 2){
    fprintf(2, "usage: xargs command [args...]\n");
    exit(1);
  }

  // Copy fixed arguments into args[]
  for(n = 1; n < argc; n++)
    args[n-1] = argv[n];

  // Read stdin one char at a time, build tokens
  char token[128];
  int tlen = 0;

  while(1){
    char c;
    int cc = read(0, &c, 1);
    if(cc < 1) break;

    if(c == '\n' || c == ' ' || c == '\t'){
      if(tlen > 0){
        token[tlen] = 0;
        args[n-1] = token;   // add argument
        args[n] = 0;         // terminate
        if(fork() == 0){
          exec(args[0], args);
          fprintf(2, "xargs: exec %s failed\n", args[0]);
          exit(1);
        }
        wait(0);
        tlen = 0;            // reset token
      }
    } else {
      if(tlen < sizeof(token)-1)
        token[tlen++] = c;
    }
  }

  // Handle trailing token (if no newline at EOF)
  if(tlen > 0){
    token[tlen] = 0;
    args[n-1] = token;
    args[n] = 0;
    if(fork() == 0){
      exec(args[0], args);
      fprintf(2, "xargs: exec %s failed\n", args[0]);
      exit(1);
    }
    wait(0);
  }

  exit(0);
}

