#include <types.h>
#include <user.h>

int main (void) {
    printf(1, "Bytes at 0x0: %p\n", *((int*)0));
    exit();
}
