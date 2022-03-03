#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#define PASS_LEN 6

struct count {
    long count; // total iterations at the moment
    pthread_mutex_t mutex;
};

struct args {
    char   *pass;
    unsigned char *md5;
    struct count *count;
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
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases

    for(long i=0; i < bound; i++) {
        long_to_pass(i, pass);

        MD5(pass, PASS_LEN, res);

        if(0 == memcmp(res, args->md5, MD5_DIGEST_LENGTH)){
            args->count->count = -1;
            args->pass = (char *) pass;
            break; // Found it!
        }
        args->count->count = i;
    }

    return (char *) pass;
}

void op_speed(struct count *count) {
    int j;
    j = count->count;
    usleep(250000); //wait a quarter of a second
    printf("\r\033[60C  %ld op/seg",(count->count-j)*4);
}

void *progress_bar(void *ptr) {
    double bound = ipow(26, PASS_LEN);
    struct count *count = ptr;
    double percent = 0;
    int i;

    //print empty bar
    printf("\r%4.2f%%",percent);
    printf("\t [");
    for(i=0;i<98;i+=2)
        printf(".");
    printf("]");

    //fill bar
    while(count->count!=-1){
        percent = (count->count / bound)*100;
        printf("\r%4.2f%%",percent);
        printf("\r\033[10C");
        for(i=2;i<=percent;i+=2)
            printf("\x1b[32m#\x1b[0m");
        printf("\r");

        op_speed(count); //operations per second

        fflush(stdout);
    }
    return NULL;
}

pthread_t start_progress(struct count *count) {
    pthread_t thread;
    if (0 != pthread_create(&thread, NULL, progress_bar, count)) {
        printf("Could not create thread");
        exit(1);
    }

    return thread;
}

int main(int argc, char *argv[]) {
    struct args *args = malloc(sizeof(struct args));
    args->count = malloc(sizeof(struct count));
    pthread_mutex_init(&args->count->mutex,NULL);
    args->count->count = 0;
    args->pass=NULL;

    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    unsigned char md5_num[MD5_DIGEST_LENGTH];
    hex_to_num(argv[1], md5_num);
    args->md5 = md5_num;

    //start progress bar
    start_progress(args->count);
    break_pass(args);

    printf("\n----------------------------------------------------------------\n");
    printf("%s: %s\n", argv[1], args->pass);
    free(args->count);
    free(args->pass);
    free(args);
    return 0;
}
