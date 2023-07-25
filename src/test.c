#include <stdio.h>

struct foo {
  int a;
  int ba;
};

struct bar {
  struct foo f;
  int c;
};

int main() {
  struct foo f = {1, 5};
  struct bar b = {f, 10};

  struct bar *bp = &b;

  printf("%d, %d, %d\n", ((struct foo*)bp)->a, ((struct foo*)bp)->ba, b.c);
  return 0;
}
