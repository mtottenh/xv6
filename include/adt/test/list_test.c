#include "double.h"

int _data_eq(elem_t a, elem_t b);
int main(void) {
    list_t l = list_alloc();
    int i = 1;
    if (l != NULL) {
        printf( "list allocated\n");
    }
    char str[] = "test";
    list_append(l, ELEM(str));
    FOR_EACH_AS_E(l) {
        printf( "Elem : %s\tsize: %d\tprev: %p\tcur: %p\tnext: %p\n",(char *)
                        ((*e)->data),(*e)->size,(*e)->prev,(*e),(*e)->next);    
    }
    list_delete(l, ELEM(str));
    FOR_EACH_AS_E(l) {
        printf( "Elem : %s\tprev: %p\tcur: %p\tnext: %p\n",(char *)
                        ((*e)->data),(*e)->prev,(*e),(*e)->next);    
    }
/*    int tmp =5;
    printf("*************************************\n");
    printf("Deleting %d\n",tmp);
    list_delete(l, ELEM(tmp)); 
    printf("*************************************\n");
    FOR_EACH_AS_E(l) {
        printf( "Elem : %d\tprev: %p\tcur: %p\tnext: %p\n",(int)
                        *(e->data),e->prev,e,e->next);    
    }
    tmp=0;
    printf("*************************************\n");
    printf("Deleting %d\n",tmp);
    list_delete(l, ELEM(tmp));
    printf("*************************************\n");
    FOR_EACH_AS_E(l) {
        printf( "Elem : %d\tprev: %p\tcur: %p\tnext: %p\n",(int)
                        *(e->data),e->prev,e,e->next);    
    }*/
}
