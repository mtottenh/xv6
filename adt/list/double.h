#include <stdio.h>
#ifndef _LIST_
#define _LIST_
#ifndef NULL
#define NULL 0
#endif 
struct list_elem {
    struct list_elem *next;
    struct list_elem *prev;
};

struct list {
    struct list_elem head;
    struct list_elem tail;
};

#define offsetof (TYPE, MEMEBER) ((size_t) &((TYPE *)0)->MEMBER)
#define list_entry(LIST_ELEM, STRUCT, MEMBER) \
        ((STRUCT *) ((uint8_t *) LIST_ELEM - offsetof(STRUCT, MEMBER)))



#define ELEM(x) elem_alloc(&x,sizeof(x))
#define FOR_EACH(l) for(elem_t e = l->head; e; e = e->next)
#define FOR_EACH_AS_E(l) for(elem_t *e = &l->head; *e; e = &((*e)->next))
typedef struct list* list_t;
typedef struct elem* elem_t;
typedef int (*cmp_func)(elem_t a,  elem_t b);

void    list_free(list_t l);
int     list_append(list_t l, elem_t e);
int     list_delete(list_t l, elem_t e);
elem_t  list_search(list_t l, void *item, cmp_func cmp);
list_t  list_alloc();
elem_t  elem_alloc(void* e, int nbytes);
#endif
