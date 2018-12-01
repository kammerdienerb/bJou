#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define N 5

typedef struct {
    int i;
    char * s;
} args_t;

void * p(void *arg) {
    args_t *args;
    int     i,
            j;
    char   *s;
    FILE   *f;

    args = arg;
    i    = args->i;
    s    = args->s;

    printf("Thread %d\n", i);

    /* f = fopen(s, "w"); */
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    for (j = 0; j < i; j += 1) {
        fprintf(stdout, "Message from thread %d\n", i);
        sleep(rand() % 3 + 1);
    }

    free(arg);

    return NULL;
}

int main() {
    pthread_t threads[N];
    int       i;
    args_t   *arg;
    void     *ret;

    srand(time(NULL));

    for (i = 0; i < N; i += 1) {
        arg = malloc(sizeof(args_t));
        arg->i = i;
        arg->s = "out.txt";
        pthread_create(&threads[i], NULL, p, arg);
    }

    for (i = 0; i < N; i += 1) {
        pthread_join(threads[i], &ret);
    }

    return 0;
}
