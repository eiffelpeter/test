/** \file

 example to allocate memory.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 200

/** This API show how to allocate and free memory.

\param argc The number of input argv.

\param argv The input parameters. \n
            argv[1] is the size want to malloc.

\return 0==OK, \n
 *      1==no input para, \n
 *      2==no mem, \n
 *      3==request too many

*/
int main(int argc, char *argv[]) {

    int ret = 0;
    unsigned char buf[100];
    char *data = NULL;
    int mallc_size = 0;

    // check input parameter
    if (argc < 2) {
        printf("please input param \n");
        ret = 2;
        goto err;
    } else {
        mallc_size = atoi(argv[1]);
        printf("malloc memory size is :%d\n", mallc_size);
    }

    if (MAX_SIZE <= mallc_size) {
        ret = 3;
        goto err;
    }

    // try to allocate memory
    data = (char *)malloc(mallc_size);
    if (data == NULL) {
        printf("can not allocate memory \n");
        ret = 1;
        goto err;
    }

    // do something
    memset(buf, 0x1, sizeof(buf));
    memset(data, 0x1, mallc_size);

    // free memory
    free(data);

    return 0;
err:
    return ret;
}
