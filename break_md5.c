#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#define PASS_LEN 6
#define NUM_THREADS 8
#define NUM_ITER 5

struct count {
    long count; // total iterations at the moment
    pthread_mutex_t mutex;
};

struct finish {
    int finish; 
    pthread_mutex_t mutex;
};

struct args {
    char   *pass;
    unsigned char *md5;
    struct count *count;
    struct finish *finish;
};

long ipow(long base, int exp) {
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1; if (!exp) break; base *= base;
    }

    return res;
}

long pass_to_long(char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
};

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *break_pass(void *ptr) {
    struct args *args = ptr;
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    int localCount;

    pthread_mutex_lock(&args->finish->mutex);
    while(args->finish->finish == 0){
        pthread_mutex_unlock(&args->finish->mutex);

        pthread_mutex_lock(&args->count->mutex);
        localCount = args->count->count;
        args->count->count += NUM_ITER;
        pthread_mutex_unlock(&args->count->mutex);

        for(int i=0; i<NUM_ITER; i++){
            long_to_pass(localCount+i, pass);

            MD5(pass, PASS_LEN, res);

            if(0 == memcmp(res, args->md5, MD5_DIGEST_LENGTH)){
                pthread_mutex_lock(&args->finish->mutex);
                args->finish->finish = 1;
                pthread_mutex_unlock(&args->finish->mutex);

                args->pass = (char *) pass;
                break; // Found it!
            }
        }

        pthread_mutex_lock(&args->finish->mutex);
    }
    pthread_mutex_unlock(&args->finish->mutex);

    return NULL;
}

void op_speed(struct count *count) {
    int j;
    j = count->count;
    usleep(250000); //wait a quarter of a second
    if(j<count->count)
        printf("\r\033[60C  %ld op/seg",(count->count-j)*4);
}

void *progress_bar(void *ptr) {
    double bound = ipow(26, PASS_LEN);
    struct args *args = ptr;
    double percent = 0;
    int i;

    //print empty bar
    printf("\r%4.2f%%",percent);
    printf("\t [");
    for(i=0;i<98;i+=2)
        printf(".");
    printf("]");

    //fill bar
    while(args->finish->finish==0){
        percent = (args->count->count / bound)*100;
        printf("\r%4.2f%%",percent);
        printf("\r\033[10C");
        for(i=2;i<=percent;i+=2)
            printf("\x1b[32m#\x1b[0m");
        printf("\r");

        op_speed(args->count); //operations per second

        fflush(stdout);
    }
    return NULL;
}

pthread_t start_progress(struct args *args) {
    pthread_t thread;
    if (0 != pthread_create(&thread, NULL, progress_bar, args)) {
        printf("Could not create thread");
        exit(1);
    }

    return thread;
}

pthread_t *start_threads(struct args *args){
    int i;

    pthread_t *threads = malloc(sizeof(pthread_t) * (NUM_THREADS));

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    // Create NUM_THREAD threads running break_pass
    for (i = 0; i < NUM_THREADS; i++) {

        if (0 != pthread_create(&threads[i], NULL, break_pass, args)) {
            printf("Could not create thread #%d of %d", i, NUM_THREADS);
            exit(1);
        }
    }

    return threads;
}

int main(int argc, char *argv[]) {
    struct args *args = malloc(sizeof(struct args));
    args->count = malloc(sizeof(struct count));
    pthread_mutex_init(&args->count->mutex,NULL);
    args->finish = malloc(sizeof(struct finish));
    pthread_mutex_init(&args->finish->mutex,NULL);
    args->count->count = 0;
    args->finish->finish = 0;

    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    unsigned char md5_num[MD5_DIGEST_LENGTH];
    hex_to_num(argv[1], md5_num);
    args->md5 = md5_num;

    //start progress bar
    pthread_t thr = start_progress(args);
    /* break_pass(args); */
    pthread_t *thrs = start_threads(args);

    pthread_join(thr, NULL);

    for(int i=0;i<NUM_THREADS;i++){
        pthread_join(thrs[i], NULL);
    }

    printf("\n----------------------------------------------------------------\n");
    printf("%s: %s\n", argv[1], args->pass);

    free(args->count);
    free(args->finish);
    free(args->pass);
    free(args);
    free(thrs);
    return 0;
}
