#include "shm_vku.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define TEST_NAME   "/vku_bench"
#define CHUNK_SIZE  4096
#define TOTAL_SIZE  (10 * 1024 * 1024) /* Truyền 10 MB dữ liệu */
#define NUM_CHUNKS  (TOTAL_SIZE / CHUNK_SIZE)

/* Hàm tính độ trễ (giây) */
double get_time_diff(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}

/* Đo lường tốc độ Shared Memory */
void benchmark_shm() {
    struct timespec start, end;
    struct shm_info shm; /* Đã sửa khớp với Dev 1 */
    int status;

    shm_destroy_vku(TEST_NAME);
    shm_create_vku(&shm, TEST_NAME, CHUNK_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &start);

    if (fork() == 0) {
        char buf[CHUNK_SIZE];
        memset(buf, 'A', CHUNK_SIZE);
        struct shm_info w;
        shm_open_vku(&w, TEST_NAME, CHUNK_SIZE);
        for (int i = 0; i < NUM_CHUNKS; i++) {
            shm_write_vku(&w, buf, CHUNK_SIZE);
        }
        shm_close_vku(&w);
        _exit(0);
    }

    if (fork() == 0) {
        char buf[CHUNK_SIZE];
        struct shm_info r;
        shm_open_vku(&r, TEST_NAME, CHUNK_SIZE);
        for (int i = 0; i < NUM_CHUNKS; i++) {
            shm_read_vku(&r, buf, CHUNK_SIZE); /* Đã sửa thành 3 tham số */
        }
        shm_close_vku(&r);
        _exit(0);
    }

    wait(&status);
    wait(&status);

    clock_gettime(CLOCK_MONOTONIC, &end);
    
    shm_close_vku(&shm);
    shm_destroy_vku(TEST_NAME);

    printf("[Shared Memory] Thoi gian truyen 10MB: %f giay\n", get_time_diff(&start, &end));
}

/* Đo lường tốc độ Pipe tiêu chuẩn */
void benchmark_pipe() {
    struct timespec start, end;
    int fd[2];
    int status;

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    if (fork() == 0) {
        close(fd[0]);
        char buf[CHUNK_SIZE];
        memset(buf, 'B', CHUNK_SIZE);
        for (int i = 0; i < NUM_CHUNKS; i++) {
            write(fd[1], buf, CHUNK_SIZE);
        }
        close(fd[1]);
        _exit(0);
    }

    if (fork() == 0) {
        close(fd[1]);
        char buf[CHUNK_SIZE];
        int n;
        while ((n = read(fd[0], buf, CHUNK_SIZE)) > 0) {}
        close(fd[0]);
        _exit(0);
    }

    close(fd[0]);
    close(fd[1]);
    wait(&status);
    wait(&status);

    clock_gettime(CLOCK_MONOTONIC, &end);
    
    printf("[Standard Pipe] Thoi gian truyen 10MB: %f giay\n", get_time_diff(&start, &end));
}

int main() {
    printf("=============================================\n");
    printf("   BENCHMARK: TRUYEN 10MB DU LIEU (IPC)\n");
    printf("=============================================\n");
    
    benchmark_pipe();
    benchmark_shm();
    
    printf("=============================================\n");
    return 0;
}
