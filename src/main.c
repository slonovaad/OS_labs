#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define THRESHOLD 100

typedef struct Context {
    int* arr;
    int* block_l;
    int* block_r;
    int* cnt;
    int pivot;
    pthread_barrier_t barrier;
    int s;
    int n_threads;
    int* a;
    int* b;
    int* end_a;
    int* end_b;
    pthread_mutex_t swap_mtx;
    int next_k;
    int m;
} Context;

typedef struct {
    int t;
    Context *ctx;
} ThreadArg;

void swap(int *arr, int i, int j) {
    int t = arr[i];
    arr[i] = arr[j];
    arr[j] = t;
}

static void insertion_sort(int *arr, int l, int r) {
    for (int i = l + 1; i < r; i++) {
        int x = arr[i], j = i - 1;
        while (j >= l && arr[j] > x) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = x;
    }
}

int partition_seq(int *arr, int l, int r) {
    int pivot = arr[l + (r - l) / 2];
    int i = l;
    swap(arr, l + (r - l) / 2, r - 1);

    for (int j = l; j < r; j++) {
        if (arr[j] <= pivot) {
            swap(arr, i, j);
            i++;
        }
    }
    return i - l;
}

void par_part_worker(int t, Context* ctx) {
    for (int i = ctx->block_l[t]; i < ctx->block_r[t]; i++) {
        if (ctx->arr[i] <= ctx->pivot) {
            ctx->cnt[t]++;
        }
    }
    pthread_barrier_wait(&ctx->barrier);
    if (t == 0) {
        for (int i = 0; i < ctx->n_threads; i++) {
            ctx->s += ctx->cnt[i];
        }
    }
    pthread_barrier_wait(&ctx->barrier);
    int ind_a = ctx->block_l[t], ind_b = ctx->block_l[t];
    for (int i = ctx->block_l[t]; i < ctx->block_r[t]; i++) {
        if (ctx->arr[i] <= ctx->pivot && i >= ctx->s) {
            ctx->a[ind_a] = i;
            ind_a++;
        } else if (ctx->arr[i] > ctx->pivot && i < ctx->s) {
            ctx->b[ind_b] = i;
            ind_b++;
        }
    }
    ctx->end_a[t] = ind_a;
    ctx->end_b[t] = ind_b;

    pthread_barrier_wait(&ctx->barrier);
    int m = 0;
    for (int i = 0; i < ctx->n_threads; i++) {
        m += ctx->end_a[i] - ctx->block_l[i];
    }
    if (t == 0) ctx->m = m;
    int chunk = m / ctx->n_threads + 1;
    int t_a = 0, t_b = 0;
    int a_ind = ctx->block_l[0], b_ind = ctx->block_l[0];
    int i = 0;
    while (1) {
        pthread_mutex_lock(&ctx->swap_mtx);
        int k = ctx->next_k;
        int k_end = k + chunk;
        if (k_end > m) k_end = m;
        ctx->next_k = k_end;
        pthread_mutex_unlock(&ctx->swap_mtx);

        if (k >= m) {
            break;
        }
        while (i < k_end) {
            if (a_ind >= ctx->block_l[t_a] && a_ind < ctx->end_a[t_a] &&
                b_ind >= ctx->block_l[t_b] && b_ind < ctx->end_b[t_b]) {
                if (i >= k) {
                    swap(ctx->arr, ctx->a[a_ind], ctx->b[b_ind]);
                }
                a_ind++;
                b_ind++;
                i++;
            }
            if (a_ind >= ctx->end_a[t_a]) {
                t_a++;
            }
            if (b_ind >= ctx->end_b[t_b]) {
                t_b++;
            }
            if (t_a >= ctx->n_threads || t_b >= ctx->n_threads) {
                break;
            }
            if (a_ind < ctx->block_l[t_a]) {
                a_ind = ctx->block_l[t_a];
            }
            if (b_ind < ctx->block_l[t_b]) {
                b_ind = ctx->block_l[t_b];
            }
        }
    }
    pthread_barrier_wait(&ctx->barrier);
}

static void *thread_main(void *arg) {
    ThreadArg *ta = (ThreadArg *)arg;
    par_part_worker(ta->t, ta->ctx);
    return NULL;
}

int partition_par(int *arr, int l, int r, int n_threads) {
    int pivot = arr[l + (r - l) / 2];
    Context ctx_storage;
    Context *ctx = &ctx_storage;

    ctx->n_threads = n_threads;
    ctx->arr = arr;

    int *block_l = malloc(n_threads * sizeof(int));
    int *block_r = malloc(n_threads * sizeof(int));
    int *cnt = malloc(n_threads * sizeof(int));
    int *end_a = malloc(n_threads * sizeof(int));
    int *end_b = malloc(n_threads * sizeof(int));
    int *a = malloc((size_t)r * sizeof(int));
    int *b = malloc((size_t)r * sizeof(int));
    if (!block_l || !block_r || !cnt || !end_a || !end_b || !a || !b) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    ctx->block_l = block_l;
    ctx->block_r = block_r;
    ctx->cnt = cnt;
    ctx->end_a = end_a;
    ctx->end_b = end_b;
    ctx->pivot = pivot;
    ctx->s = l;
    ctx->next_k = 0;
    ctx->m = 0;
    ctx->a = a;
    ctx->b = b;
    for (int i = 0; i < n_threads; i++) {
        ctx->block_l[i] = l + (r - l) * i / n_threads;
        ctx->block_r[i] = l + (r - l) * (i + 1) / n_threads;
        ctx->cnt[i] = 0;
    }
    ctx->block_r[n_threads - 1] = r;

    pthread_barrier_init(&ctx->barrier, NULL, n_threads);
    pthread_mutex_init(&ctx->swap_mtx, NULL);

    pthread_t *threads = NULL;
    ThreadArg *args = NULL;
    if (n_threads > 1) {
        threads = malloc((n_threads - 1) * sizeof(pthread_t));
        args = malloc((n_threads - 1) * sizeof(ThreadArg));
        if (!threads || !args) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        for (int i = 1; i < n_threads; i++) {
            args[i - 1].t = i;
            args[i - 1].ctx = ctx;
            if (pthread_create(&threads[i - 1], NULL, thread_main, &args[i - 1]) != 0) {
                fprintf(stderr, "pthread_create failed\n");
                exit(1);
            }
        }
    }

    par_part_worker(0, ctx);

    if (n_threads > 1) {
        for (int i = 1; i < n_threads; i++)
            pthread_join(threads[i - 1], NULL);
    }

    pthread_barrier_destroy(&ctx->barrier);
    pthread_mutex_destroy(&ctx->swap_mtx);

    int result = ctx->s - l;

    free(block_l);
    free(block_r);
    free(cnt);
    free(end_a);
    free(end_b);
    free(a);
    free(b);
    free(threads);
    free(args);

    return result;
}

void quicksort_seq(int *arr, int l, int r) {
    while (r - l > THRESHOLD) {
        int s = partition_seq(arr, l, r);
        if (s == 0 || s == r - l) {
            insertion_sort(arr, l, r);
            return;
        }
        if (s < (r - l) - s) {
            quicksort_seq(arr, l, l + s);
            l = l + s;
        } else {
            quicksort_seq(arr, l + s, r);
            r = l + s;
        }
    }
    insertion_sort(arr, l, r);
}
void sort_parallel(int *arr, int l, int r, int n_threads) {
    if (r - l <= THRESHOLD) {
        insertion_sort(arr, l, r);
        return;
    }
    int s = partition_par(arr, l, r, n_threads);
    if (s == 0 || s == r - l) {
        quicksort_seq(arr, l, r);
        return;
    }
    sort_parallel(arr, l, l + s, n_threads);
    sort_parallel(arr, l + s, r, n_threads);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <threads>\n", argv[0]);
        return 1;
    }
    int nt = atoi(argv[1]);
    if (nt < 1) {
        fprintf(stderr, "threads must be positive\n");
        return 1;
    }

    int n;
    if (scanf("%d", &n) != 1 || n < 1) {
        fprintf(stderr, "bad array size\n");
        return 1;
    }

    int *arr = malloc((size_t)n * sizeof(int));
    if (!arr) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &arr[i]) != 1) {
            fprintf(stderr, "bad input\n");
            free(arr);
            return 1;
        }
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sort_parallel(arr, 0, n, nt);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    fprintf(stderr, "n=%d threads=%d time=%.6f\n", n, nt, dt);

    // for (int i = 0; i < n; i++) {
    //     printf("%d ", arr[i]);
    // }
    printf("\n");
    free(arr);
    return 0;
}