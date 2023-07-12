#include <stdint.h>
#include <stdio.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-server-core.h>

struct myListElem {
  struct wl_list link;
  int num;
};

int main(int argc, char *argv[]) {
  struct wl_list *foo_list = calloc(0, sizeof(struct myListElem));
  struct myListElem e1, e2, e3, e4;

  e1.num = 1;
  e2.num = 2;
  e3.num = 3;
  e4.num = 4;

  wl_list_init(foo_list);
  wl_list_insert(foo_list, &e1.link); // [1]
  wl_list_insert(&e1.link, &e2.link); // [1]
  wl_list_insert(&e1.link, &e3.link);
  //wl_list_insert(foo_list, &e3.link); // [1]
  //  wl_list_insert(foo_list, &e2.link); // [2, 1]
  /* wl_list_insert(&e2.link, &e3.link);  // [2, 3, 1] */
  /* wl_list_insert(&e1.link, &e4.link);  // [2, 3, 1, 4] */

  /* wl_list_init(&foo_list); */
  /* wl_list_insert(&foo_list, &e1.link); */
  /* wl_list_insert(&e1.link, &e2.link); */

  struct myListElem *i = foo_list;
  int a = 100;
  while(a){
    printf("%d ", i->num);
    i = wl_container_of(i->link.next, i, link);
    a--;
  }

  printf("\n");


  struct myListElem *e;
  wl_list_for_each(e, foo_list, link) {
    if(!e->link.next) break;
    int nextNum = ((struct myListElem*)e->link.next)->num;
    int prevNum = ((struct myListElem*)e->link.prev)->num;

    printf("%d",e->num);
  }

  printf("done\n");

  return 0;
}
