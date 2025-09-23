#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

char* fmtname(char *path) {
  static char buf[DIRSIZ+1];
  char *p;

  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

void run_exec(char *path, char **cmdargv, int cmdargc) {
  if(fork() == 0) {
    char *argv[MAXARG];
    int i;

    for(i = 0; i < cmdargc; i++)
      argv[i] = cmdargv[i];

    argv[cmdargc] = path;
    argv[cmdargc+1] = 0;

    exec(argv[0], argv);
    fprintf(2, "find: exec %s failed\n", argv[0]);
    exit(1);
  }
  wait(0);
}

void find(char *path, char *target, int do_exec, char **cmdargv, int cmdargc) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type == T_FILE){
    if(strcmp(fmtname(path), target) == 0) {
      if(do_exec)
        run_exec(path, cmdargv, cmdargc);
      else
        printf("%s\n", path);
    }
  } else if(st.type == T_DIR){
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
    } else {
      strcpy(buf, path);
      p = buf+strlen(buf);
      *p++ = '/';
      while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
          continue;
        if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        find(buf, target, do_exec, cmdargv, cmdargc);
      }
    }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if(argc < 3){
    fprintf(2, "usage: find <path> <filename> [-exec command...]\n");
    exit(1);
  }

  if(argc == 3) {
    find(argv[1], argv[2], 0, 0, 0);
  } else {
    if(strcmp(argv[3], "-exec") != 0) {
      fprintf(2, "usage: find <path> <filename> [-exec command...]\n");
      exit(1);
    }
    char **cmdargv = &argv[4];
    int cmdargc = argc - 4;
    if(cmdargc < 1) {
      fprintf(2, "find: missing command after -exec\n");
      exit(1);
    }
    find(argv[1], argv[2], 1, cmdargv, cmdargc);
  }

  exit(0);
}

