#include "kernel/types.h"
#include "user/user.h"


#define MAXCAND 256

static char *cands_ptrs[MAXCAND];
static int  cands_len[MAXCAND];

static int is_alnum(char c) {
  if (c >= '0' && c <= '9') return 1;
  if (c >= 'A' && c <= 'Z') return 1;
  if (c >= 'a' && c <= 'z') return 1;
  return 0;
}
static int has_upper(const char *s, int len) {
  int i;
  for (i = 0; i < len; i++) if (s[i] >= 'A' && s[i] <= 'Z') return 1;
  return 0;
}
static int has_lower(const char *s, int len) {
  int i;
  for (i = 0; i < len; i++) if (s[i] >= 'a' && s[i] <= 'z') return 1;
  return 0;
}
static int has_digit(const char *s, int len) {
  int i;
  for (i = 0; i < len; i++) if (s[i] >= '0' && s[i] <= '9') return 1;
  return 0;
}

static void scan_region(char *base, int bytes, int *cand_count) {
  int i = 0;
  while (i < bytes) {
    if (!is_alnum(base[i])) { i++; continue; }
    int j = i;
    while (j < bytes && is_alnum(base[j])) j++;
    int len = j - i;
    if (len >= 4 && len <= 1024) {
      if (*cand_count < MAXCAND) {
        cands_ptrs[*cand_count] = base + i;
        cands_len[*cand_count] = len;
        (*cand_count)++;
      }
    }
    if (j == i) i++; else i = j;
  }
}

int main(int argc, char *argv[]) {
  int first = 256 * 1024;
  int extra = 256 * 1024;

  char *p = (char*) sbrk(first);
  if (p == (char*) -1) exit(1);
  int total = first;

  int cand_count = 0;
  scan_region(p, total, &cand_count);

  if (cand_count == 0) {
    char *q = (char*) sbrk(extra);
    if (q != (char*) -1) {
      total += extra;
      scan_region(p, total, &cand_count);
    }
  }

  int chosen = -1;
  int pass, k;
  for (pass = 0; pass < 2 && chosen == -1; pass++) {
    for (k = 0; k < cand_count; k++) {
      int L = cands_len[k];
      if (L < 6 || L > 64) continue;
      char *s = cands_ptrs[k];
      if (pass == 0) {
        if (has_upper(s, L) && has_lower(s, L)) { chosen = k; break; }
      } else {
        if ((has_upper(s, L) || has_lower(s, L)) && has_digit(s, L)) { chosen = k; break; }
      }
    }
  }
  if (chosen == -1) {
    int Lbest = 0;
    for (k = 0; k < cand_count; k++) {
      if (cands_len[k] > Lbest) { Lbest = cands_len[k]; chosen = k; }
    }
  }

  if (chosen != -1) {
    int L = cands_len[chosen];
    if (L > 1023) L = 1023;
    char buf[1024];
    int i;
    for (i = 0; i < L; i++) buf[i] = cands_ptrs[chosen][i];
    buf[L] = '\0';
    printf("%s\n", buf);
    exit(0);
  }

  exit(1);
}

