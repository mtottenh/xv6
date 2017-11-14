#include "list.h"
#include <stdlib.h>
#include <string.h>


elem_t elem_alloc(void* data, int nbytes) {
    elem_t e = malloc(nbytes + sizeof(struct elem));
    memset(e, 0, nbytes + sizeof (struct elem));
    memcpy(e->data,data,nbytes);
    e->size = nbytes;
    return e;
}

void list_free(list_t l) {
    elem_t *e = &l->head;
    elem_t n = 0;
    while(*e) {
        n = (*e)->next;
        free(*e);
        e = &n;
    }
    free(l);
}

int  list_append(list_t l, elem_t e) {
    if (l == NULL)
        return 1;
    elem_t* n = &(l->tail);
    if (*n) {
        l->tail->next = e;
        e->prev = l->tail;
    }
    *n = e;
    n = &(l->head);
    if (!(*n))
        *n = e;
    return 0;
}


int _data_eq (elem_t a, elem_t b) {
    if (a->size < b->size)
        return 1;
    if (a->size > b->size)
        return 1;
    return memcmp(a->data, b->data, a->size);
}

int  list_delete(list_t l, elem_t e) {

    elem_t* node = &l->head;
    elem_t* prev;

    for (; *node && _data_eq(e,(*node)) != 0; 
                    node = &(*node)->next) {
        
    }
    if (*node) {
        elem_t n = *node;
        prev = &((*node)->prev); 
/*        printf( "Elem : %d\tprev: %p\tcur: %p\tnext: %p\n",(int)
                        *((*node)->data),(*node)->prev,*node,(*node)->next);    */
        if (*prev) {
            (*prev)->next = (*node)->next;
            (*node)->next->prev = (*prev);
        } else {
            (*node) = (*node)->next;
        }
/*        printf( "Elem : %d\tprev: %p\tcur: %p\tnext: %p\n",(int)
                        *((*node)->data),(*node)->prev,*node,(*node)->next);   */ 
        free(n);
        n = NULL;
    } 
    return 0;
}

elem_t  list_search(list_t l, void *item, cmp_func cmp) {
    FOR_EACH_AS_E(l) {
        if (cmp(item,e) == 0) {
            return e;
        }
    }
    return NULL;
}

list_t list_alloc() {
    list_t l = malloc(sizeof (struct list));
    memset (l, 0, sizeof (struct list));
    
    return l;
}
