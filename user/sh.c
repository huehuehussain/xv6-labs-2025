
// user/sh.c - xv6 shell with:
//  - suppressed prompt when stdin is not console
//  - builtin 'wait' to wait for all children
//  - simple history (history, !n)
// This is a small conservative extension of the original xv6 shell.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"

#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 32
#define MAXLINE 512
#define HISTSZ 32

// ----- original shell data structures -----

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

// forward
int fork1(void);
void panic(char*);
struct cmd* parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// ----- runcmd (original) -----

void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

// ----- helpers -----

int
fork1(void)
{
  int pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

// constructors (original)
struct cmd* execcmd(void);
struct cmd* redircmd(struct cmd*, char*, char*, int, int);
struct cmd* pipecmd(struct cmd*, struct cmd*);
struct cmd* listcmd(struct cmd*, struct cmd*);
struct cmd* backcmd(struct cmd*);

struct cmd*
execcmd(void)
{
  struct execcmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}

// ----- parsing (original) -----

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >> append
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}

// ----- SIMPLE HISTORY & BUILTIN WAIT -----
// history stores lines (no large features)
static char history[HISTSZ][MAXLINE];
static int hist_head = 0;
static int hist_count = 0;
static int hist_next_index = 1;

static void hist_add(char *line) {
  if(!line || *line == 0) return;
  int len = (int)strlen(line);
  if(len >= MAXLINE) len = MAXLINE-1;
  memmove(history[hist_head], line, len);
  history[hist_head][len] = 0;
  hist_head = (hist_head + 1) % HISTSZ;
  if(hist_count < HISTSZ) hist_count++;
  hist_next_index++;
}

static void hist_print(void) {
  int start = (hist_head - hist_count + HISTSZ) % HISTSZ;
  int idx = hist_next_index - hist_count;
  for(int i = 0; i < hist_count; i++) {
    int p = (start + i) % HISTSZ;
    printf("%d\t%s\n", idx + i, history[p]);
  }
}

static char* hist_get_by_number(int number) {
  int base = hist_next_index - hist_count;
  if(number < base || number >= hist_next_index) return 0;
  int offset = number - base;
  int p = ( (hist_head - hist_count + offset) % HISTSZ + HISTSZ) % HISTSZ;
  return history[p];
}

// detect interactive: compare fd0 to "console"
static int is_interactive(void) {
  struct stat s0, sc;
  if(fstat(0, &s0) < 0) return 0;
  if(stat("console", &sc) < 0) return 0;
  return s0.dev == sc.dev && s0.ino == sc.ino;
}

// getcmd: prints prompt only when interactive
int
getcmd(char *buf, int nbuf)
{
  if(is_interactive()){
    write(2, "$ ", 2);
  }
  memset(buf, 0, nbuf);
  if(gets(buf, nbuf) <= 0) // EOF
    return -1;
  return 0;
}

static int is_builtin_cmd(char **argv) {
  if(argv[0] == 0) return 0;
  if(strcmp(argv[0], "cd") == 0) return 1;
  if(strcmp(argv[0], "exit") == 0) return 1;
  if(strcmp(argv[0], "wait") == 0) return 1;
  if(strcmp(argv[0], "history") == 0) return 1;
  return 0;
}

static void run_builtin_cmd(char **argv) {
  if(strcmp(argv[0], "cd") == 0){
    if(argv[1] == 0){
      fprintf(2, "cd: missing argument\n");
    } else if(chdir(argv[1]) < 0){
      fprintf(2, "cd: cannot cd %s\n", argv[1]);
    }
    return;
  }
  if(strcmp(argv[0], "exit") == 0){
    exit(0);
  }
  if(strcmp(argv[0], "wait") == 0){
    while(wait((int*)0) > 0)
      ;
    return;
  }
  if(strcmp(argv[0], "history") == 0){
    hist_print();
    return;
  }
}

// ----- MAIN -----
// Conservative main loop: preserves original semantics, adds !n and history and builtin wait
int
main(void)
{
  static char buf[MAXLINE];
  int fd;

  // Ensure that file descriptors 0,1,2 are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmdline = buf;
    // skip leading whitespace
    while(*cmdline && (*cmdline == ' ' || *cmdline == '\t')) cmdline++;

    if(*cmdline == 0) continue;

    // Support !n : immediately substitute line with history entry
    if(cmdline[0] == '!' && cmdline[1] >= '0' && cmdline[1] <= '9'){
      int num = atoi(cmdline+1);
      char *h = hist_get_by_number(num);
      if(h){
        // copy history entry into cmdline buffer
        int len = (int)strlen(h);
        if(len >= MAXLINE) len = MAXLINE-1;
        memmove(buf, h, len);
        buf[len] = 0;
        cmdline = buf;
        printf("%s\n", cmdline); // echo the substituted command for visibility
      } else {
        fprintf(2, "no such history entry %d\n", num);
        continue;
      }
    }

    // Save to history
    hist_add(cmdline);

    // special-case cd: parent must do it (like original)
    if(cmdline[0]=='c' && cmdline[1]=='d' && (cmdline[2]==' ' || cmdline[2]=='\t')){
      // chop newline
      int L = strlen(cmdline);
      if(L > 0 && cmdline[L-1] == '\n') cmdline[L-1] = 0;
      char *path = cmdline + 3;
      while(*path && (*path == ' ' || *path == '\t')) path++;
      if(chdir(path) < 0){
        fprintf(2, "cannot cd %s\n", path);
      }
      continue;
    }

    // Parse command
    struct cmd *cmd = parsecmd(cmdline);
    if(cmd == 0) continue;

    // If it's a simple EXEC command and builtin, run in parent (so cd works, etc).
    if(cmd->type == EXEC){
      struct execcmd *ec = (struct execcmd*)cmd;
      if(ec->argv[0] && is_builtin_cmd(ec->argv)){
        run_builtin_cmd(ec->argv);
        continue;
      }
    }

    // Otherwise fork and run command (original behavior)
    if(fork1() == 0)
      runcmd(cmd);
    wait(0);
  }
  exit(0);
}

