#include "stdio.h"
#include "cc/array.h"
#include "assert.h"

void array_destroy_cb_handle(void *item)
{
    uint32_t *n = item;
    printf("Destroying %u\n", *n);
    free(n);
}

int main(int argc, char **argv)
{
    Array *ar;
    enum cc_stat st;
    uint8_t i;
    ArrayIter it;

    st = array_new(&ar);
    assert(st == CC_OK);

    array_iter_init(&it, ar);

    for (i = 0; i < 10; ++i) {
        uint32_t *n = malloc(sizeof(uint32_t));
        assert(n);
        *n = i;
        array_add(ar, n);
    }

    array_destroy_cb(ar, array_destroy_cb_handle);

    printf("collections-c sample done\n");

    return 0;
}
