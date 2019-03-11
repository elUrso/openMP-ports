/*requires -lssl -lcrypto*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "bench.h"

void process_init() {
    bench_data.out = (char *) malloc(512);
    bench_data.out_size = 0;
    bench_data.out_max = 512;

    sha256_init(&bench_data.sha_ctx);
}

void process_append_result(char * str, int size) {
    int s_size, n_size;
    s_size = size;
    n_size = s_size + bench_data.out_size;
    if(n_size >= bench_data.out_max) {
        do {
            bench_data.out_max = bench_data.out_max*2;
        } while(n_size >= bench_data.out_max);
        bench_data.out = (char *) realloc(bench_data.out,   bench_data.out_max);
    }
    memcpy(bench_data.out + bench_data.out_size, str, s_size);
    bench_data.out_size = n_size;
}

void process_append_file(char * str) {
    FILE * out_file = fopen(str, "rb");
    char out_bytes[128];
    int out_size;
    out_size = fread(out_bytes, 1, 128, out_file);
    while(out_size > 0) {
        process_append_result(out_bytes, out_size);
        out_size = fread(out_bytes, 1, 128, out_file);
    }
    fclose(out_file);
}

#ifdef _OPENMP
#include <omp.h>

#define BENCH_TPT (4096) /*Number of tasks per thread to be remembered*/

static unsigned long long **pool;
static int *ptr;
static int *loop;

#endif

static int str_size(char * str) {
    int i;
    for(i = 0; str[i] != '\0'; i++) {
        switch(str[i]) {
            case '\b':
            case '\\':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
            case '\"':
                i++;
        }
    };
    return i+1;
};

char * mode[] = {"SEQ", "OPENMP", "PTHREADS", "OPTMIZED", "CUDA", "OPENMP_TASK", "OMPSS", "OMPSS2"};

static double rtclock()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME,&t);
    return t.tv_sec  + t.tv_nsec *1e-9;
}

static unsigned long long clk_timing(void) {
	unsigned long lo, hi;
	unsigned long long l, h;
	asm("rdtsc" : "=a"(lo), "=d"(hi)); 
	h = hi;
	l = lo | (h << 32);
	return l;
}

void process_name(char * str) {
    bench_data.name = str;
}

void process_mode(enum Bench_mode mode) {
    bench_data.mode = mode;
}

int process_args(int argc, char **argv) {
    int i = 0;
    for(int s = 0; s < argc; s++)
        i += str_size(argv[s]);
    char * q = (char *) malloc(i);
    if(q == NULL) return 0;
    i = 0;
    for(int s = 0; s < argc; s++) {
        for(int j = 0; argv[s][j] != '\0'; j++) {
            switch(argv[s][j]) {
            case '\b':
                q[i++] = '\\';
                q[i++] = 'b';
                break;
            case '\\':
                q[i++] = '\\';
                q[i++] = '\\';
                break;
            case '\f':
                q[i++] = '\\';
                q[i++] = 'f';
                break;
            case '\n':
                q[i++] = '\\';
                q[i++] = 'n';
                break;
            case '\r':
                q[i++] = '\\';
                q[i++] = 'r';
                break;
            case '\t':
                q[i++] = '\\';
                q[i++] = 't';
                break;
            case '\"':
                q[i++] = '\\';
                q[i++] = '"';
                break;
            default:
                q[i++] = argv[s][j];
            }
        }
        q[i++] = ' ';
    }
    q[--i] = '\0';
    bench_data.args = q;
    return 1;
}

int process_start_measure(void) {
    bench_data.begin = rtclock();
    return 1;
}

int process_stop_measure(void) {
    bench_data.end = rtclock();
    return 1;
}

int dump_csv(FILE * f) {
    fprintf(f, "{\"bench\" : \"%s\", \"id\" : \"\",\"mode\" : \"%s\",\"args\" : \"%s\",\"time\" : %lf", bench_data.name, mode[bench_data.mode], bench_data.args, bench_data.end - bench_data.begin);
    
    #ifdef _OPENMP
    fprintf(f, ", \"tasks\" : [");
    int q = omp_get_num_threads();
    for(int i = 0; i < q; i++) {
        if(loop[i]) {
            for(int j = 0; j < BENCH_TPT; j++) {
                printf("%llu,", pool[i][j]);
            }
        } else {
            for(int j = 0; j < ptr[i]; j++) {
                printf("%llu,", pool[i][j]);
            }
        }
    }
    fprintf(f,"0 ]");
    #else
    fprintf(f, ", \"tasks\" : \"not available\"");
    #endif

    #ifdef DEBUG
    puts(bench_data.out);
    #endif

    fprintf(f, ",\"output\" : \"");

    BYTE buf[SHA256_BLOCK_SIZE];
	sha256_update(&bench_data.sha_ctx, bench_data.out, bench_data.out_size);
    sha256_final(&bench_data.sha_ctx, buf);
	for(int i = 0; i < SHA256_BLOCK_SIZE; i++)
		fprintf(f, "%02x", buf[i]);
    fprintf(f, "\"");
    
    fprintf(f, "}\n");
    return 1;
}

#ifdef _OPENMP

void * pmalloc(size_t size) {
    void * q = malloc(size);
    if(q == NULL) exit(EXIT_FAILURE);
    return q;
}

int task_init_measure(void) {
    int q = omp_get_num_threads();
   
    pool = (unsigned long long int **) pmalloc(sizeof(int *) * q);
    ptr = (int *) pmalloc(sizeof(int) * q);
    loop = (int *) pmalloc(sizeof(int) * q);
    for(int i = 0; i < q; i++) {
        pool[i] = (unsigned long long int *) pmalloc(sizeof(unsigned long long) * BENCH_TPT);
        ptr[i] = 0;
        loop[i] = 0;
    }
}

int task_stop_measure(void) {
    int q = omp_get_thread_num();
    pool[q][ptr[q]] = clk_timing() - pool[q][ptr[q]];
    ptr[q]++;
    if(ptr[q] >= BENCH_TPT) {
        ptr[q] = 0;
        loop[q] = 1;
    }
}

int task_start_measure(void) {
    int q = omp_get_thread_num();
    pool[q][ptr[q]] = clk_timing();
}

#endif
