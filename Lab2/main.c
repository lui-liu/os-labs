#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int rows, cols;
int w;
int K;
double *kernel;
pthread_barrier_t barrier;

typedef struct {
    double *src, *dst;
    int start, end;
} ThreadArgs;

void die(const char *msg)
{
    fprintf(stderr, "Ошибка: %s\n", msg);
    exit(EXIT_FAILURE);
}

double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

void print_matrix(FILE *f, const char *title, const double *m)
{
    fprintf(f, "%s:\n", title);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            fprintf(f, "%8.2f ", m[i * cols + j]);
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}

int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void convolve_rows(const double *src, double *dst, int start, int end)
{
    int r = w / 2;

    for (int i = start; i < end; i++) {
        for (int j = 0; j < cols; j++) {
            double sum = 0.0;
            for (int di = -r; di <= r; di++) {
                for (int dj = -r; dj <= r; dj++) {
                    int y = clamp(i + di, 0, rows - 1);
                    int x = clamp(j + dj, 0, cols - 1);
                    sum += src[y * cols + x] * kernel[(di + r) * w + (dj + r)];
                }
            }
            dst[i * cols + j] = sum;
        }
    }
}

void run_sequential(double *src, double *dst)
{
    for (int k = 0; k < K; k++) {
        convolve_rows(src, dst, 0, rows);
        double *tmp = src; src = dst; dst = tmp;
    }
}

void save_steps(const char *filename, const double *input)
{
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        perror("Не удалось открыть файл для записи шагов");
        return;
    }

    size_t bytes = (size_t)rows * cols * sizeof(double);
    double *src = malloc(bytes), *dst = malloc(bytes);
    if (src == NULL || dst == NULL)
        die("не хватает памяти для записи шагов");
    memcpy(src, input, bytes);

    fprintf(f, "Матрица %dx%d, окно %dx%d, K = %d\n\n", rows, cols, w, w, K);
    print_matrix(f, "Исходная матрица", src);

    for (int k = 0; k < K; k++) {
        convolve_rows(src, dst, 0, rows);
        double *tmp = src; src = dst; dst = tmp;

        char title[64];
        snprintf(title, sizeof(title), "После итерации %d", k + 1);
        print_matrix(f, title, src);
    }

    fclose(f);
    free(src);
    free(dst);
    printf("Шаги свёртки записаны в файл %s\n", filename);
}

void *worker(void *arg)
{
    ThreadArgs *t = arg;
    double *src = t->src, *dst = t->dst;

    for (int k = 0; k < K; k++) {
        convolve_rows(src, dst, t->start, t->end);
        pthread_barrier_wait(&barrier);
        double *tmp = src; src = dst; dst = tmp;
    }
    return NULL;
}

void run_parallel(double *src, double *dst, int threads)
{
    pthread_t *ids = malloc(threads * sizeof(pthread_t));
    ThreadArgs *args = malloc(threads * sizeof(ThreadArgs));
    if (ids == NULL || args == NULL)
        die("не хватает памяти под потоки");

    if (pthread_barrier_init(&barrier, NULL, threads) != 0)
        die("не удалось создать барьер");

    int per_thread = rows / threads;
    int extra = rows % threads;
    int start = 0;

    for (int i = 0; i < threads; i++) {
        int count = per_thread + (i < extra ? 1 : 0);
        args[i].src = src;
        args[i].dst = dst;
        args[i].start = start;
        args[i].end = start + count;
        start += count;

        int rc = pthread_create(&ids[i], NULL, worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "Ошибка pthread_create: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < threads; i++)
        pthread_join(ids[i], NULL);

    pthread_barrier_destroy(&barrier);
    free(ids);
    free(args);
}

int main(int argc, char *argv[])
{
    if (argc != 6) {
        fprintf(stderr, "Использование: %s <потоки> <строки> <столбцы> <окно> <K>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int threads = atoi(argv[1]);
    rows = atoi(argv[2]);
    cols = atoi(argv[3]);
    w = atoi(argv[4]);
    K = atoi(argv[5]);

    if (threads <= 0 || rows <= 0 || cols <= 0 || w <= 0 || K <= 0)
        die("все параметры должны быть положительными числами");
    if (w % 2 == 0)
        die("размер окна должен быть нечётным");

    size_t bytes = (size_t)rows * cols * sizeof(double);
    double *input = malloc(bytes);
    double *seq_a = malloc(bytes), *seq_b = malloc(bytes);
    double *par_a = malloc(bytes), *par_b = malloc(bytes);
    kernel = malloc(w * w * sizeof(double));
    if (!input || !seq_a || !seq_b || !par_a || !par_b || !kernel)
        die("не хватает памяти под матрицы");

    srand(42);
    for (int i = 0; i < rows * cols; i++)
        input[i] = rand() % 10000 / 100.0;
    for (int i = 0; i < w * w; i++)
        kernel[i] = 1.0 / (w * w);

    memcpy(seq_a, input, bytes);
    memcpy(par_a, input, bytes);

    printf("PID: %d (потоки: ps -T -p %d)\n", getpid(), getpid());

    double t0 = now_ms();
    run_sequential(seq_a, seq_b);
    double t_seq = now_ms() - t0;

    t0 = now_ms();
    run_parallel(par_a, par_b, threads);
    double t_par = now_ms() - t0;

    double *seq_res = (K % 2 == 0) ? seq_a : seq_b;
    double *par_res = (K % 2 == 0) ? par_a : par_b;
    int same = memcmp(seq_res, par_res, bytes) == 0;

    if (rows <= 20 && cols <= 20)
        save_steps("steps.txt", input);

    double S = t_seq / t_par;
    double X = S / threads;

    printf("Последовательно:   %.3f мс\n", t_seq);
    printf("Параллельно (%d):  %.3f мс\n", threads, t_par);
    printf("Ускорение S:       %.3f\n", S);
    printf("Эффективность X:   %.4f\n", X);
    printf("Результаты %s\n", same ? "совпадают" : "НЕ СОВПАДАЮТ!");

    free(input); free(kernel);
    free(seq_a); free(seq_b);
    free(par_a); free(par_b);
    return same ? EXIT_SUCCESS : EXIT_FAILURE;
}