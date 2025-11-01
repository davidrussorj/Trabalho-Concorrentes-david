// src/image_threads.c
// Compile: gcc src/image_threads.c -o image_threads -lpthread -O3 -Wall -Wextra
// Trabalho: Implementação de Versões Concorrentes (A, B e C)
//
// Versão A: fatias horizontais (atribuição estática)
// Versão B: blocos/tiling (atribuição estática)  ← Feedback 1
// Versão C: blocos/tiling (atribuição dinâmica / pool) ← Feedback 4

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define THRESHOLD 128
#define VERSAO_A 1
#define VERSAO_B 2
#define VERSAO_C 3

typedef struct {
    int width;
    int height;
    unsigned char *data;
} Image;

static long long g_contador_global = 0;
static pthread_mutex_t g_mutex_global = PTHREAD_MUTEX_INITIALIZER;

static inline int pixel_match(unsigned char p) {
    return p > THRESHOLD;
}

// Fase 0: carregar imagem (mock)
static Image *carregar_imagem_fake(int w, int h) {
    Image *img = (Image *)malloc(sizeof(Image));
    img->width = w;
    img->height = h;
    img->data = (unsigned char *)malloc((size_t)w * h);
    for (long long i = 0; i < (long long)w * h; i++) {
        img->data[i] = (unsigned char)(rand() % 256);
    }
    return img;
}

/* ───────────────────────────────────────────────
 * VERSÃO A – fatias horizontais estáticas
 * ─────────────────────────────────────────────── */
typedef struct {
    int thread_id;
    Image *img;
    int linha_ini;
    int linha_fim;
} ThreadArgsA;

static void *thread_func_a(void *arg) {
    ThreadArgsA *a = (ThreadArgsA *)arg;
    long long local = 0;
    for (int y = a->linha_ini; y < a->linha_fim; y++) {
        int off = y * a->img->width;
        for (int x = 0; x < a->img->width; x++) {
            unsigned char p = a->img->data[off + x];
            if (pixel_match(p)) local++;
        }
    }
    pthread_mutex_lock(&g_mutex_global);
    g_contador_global += local;
    pthread_mutex_unlock(&g_mutex_global);
    return NULL;
}

static void versao_a(Image *img, int n_threads) {
    pthread_t *ths = (pthread_t *)malloc((size_t)n_threads * sizeof(pthread_t));
    ThreadArgsA *args = (ThreadArgsA *)malloc((size_t)n_threads * sizeof(ThreadArgsA));
    int base = img->height / n_threads;
    int resto = img->height % n_threads;
    int linha = 0;
    for (int i = 0; i < n_threads; i++) {
        int ini = linha;
        int fim = ini + base + (i == n_threads - 1 ? resto : 0);
        args[i].thread_id = i;
        args[i].img = img;
        args[i].linha_ini = ini;
        args[i].linha_fim = fim;
        pthread_create(&ths[i], NULL, thread_func_a, &args[i]);
        linha = fim;
    }
    for (int i = 0; i < n_threads; i++) pthread_join(ths[i], NULL);
    free(ths); free(args);
}

/* ───────────────────────────────────────────────
 * Comum às versões B e C (processar bloco)
 * ─────────────────────────────────────────────── */
static void processar_bloco(Image *img, int bx, int by, int bw, int bh, long long *local) {
    int x_ini = bx * bw;
    int y_ini = by * bh;
    int x_fim = x_ini + bw;
    int y_fim = y_ini + bh;

    if (x_fim > img->width)  x_fim = img->width;
    if (y_fim > img->height) y_fim = img->height;

    for (int y = y_ini; y < y_fim; y++) {
        int off = y * img->width;
        for (int x = x_ini; x < x_fim; x++) {
            unsigned char p = img->data[off + x];
            if (pixel_match(p)) (*local)++;
        }
    }
}

/* ───────────────────────────────────────────────
 * VERSÃO B – blocos (tiling) estáticos
 * ─────────────────────────────────────────────── */
typedef struct {
    int thread_id;
    Image *img;
    int bloco_ini;
    int bloco_fim;
    int bloco_w;
    int bloco_h;
    int n_blocos_x;
} ThreadArgsB;

static void *thread_func_b(void *arg) {
    ThreadArgsB *a = (ThreadArgsB *)arg;
    long long local = 0;
    for (int b = a->bloco_ini; b < a->bloco_fim; b++) {
        int by = b / a->n_blocos_x;
        int bx = b % a->n_blocos_x;
        processar_bloco(a->img, bx, by, a->bloco_w, a->bloco_h, &local);
    }
    pthread_mutex_lock(&g_mutex_global);
    g_contador_global += local;
    pthread_mutex_unlock(&g_mutex_global);
    return NULL;
}

static void versao_b(Image *img, int n_threads, int bw, int bh) {
    int n_blocos_x = (img->width  + bw - 1) / bw;
    int n_blocos_y = (img->height + bh - 1) / bh;
    int total_blocos = n_blocos_x * n_blocos_y;

    pthread_t *ths = (pthread_t *)malloc((size_t)n_threads * sizeof(pthread_t));
    ThreadArgsB *args = (ThreadArgsB *)malloc((size_t)n_threads * sizeof(ThreadArgsB));

    int base = total_blocos / n_threads;
    int resto = total_blocos % n_threads;
    int cur = 0;

    for (int i = 0; i < n_threads; i++) {
        int ini = cur;
        int fim = ini + base + (i == n_threads - 1 ? resto : 0);
        args[i].thread_id = i;
        args[i].img = img;
        args[i].bloco_ini = ini;
        args[i].bloco_fim = fim;
        args[i].bloco_w = bw;
        args[i].bloco_h = bh;
        args[i].n_blocos_x = n_blocos_x;
        pthread_create(&ths[i], NULL, thread_func_b, &args[i]);
        cur = fim;
    }

    for (int i = 0; i < n_threads; i++) pthread_join(ths[i], NULL);
    free(ths); free(args);
}

/* ───────────────────────────────────────────────
 * VERSÃO C – blocos (tiling) dinâmicos (pool)
 * ─────────────────────────────────────────────── */
typedef struct {
    int thread_id;
    Image *img;
    int total_blocos;
    int bloco_w;
    int bloco_h;
    int n_blocos_x;
} ThreadArgsC;

static pthread_mutex_t g_mutex_tasks = PTHREAD_MUTEX_INITIALIZER;
static int g_proximo_bloco = 0;

static void *thread_func_c(void *arg) {
    ThreadArgsC *a = (ThreadArgsC *)arg;
    long long local = 0;

    for (;;) {
        int id = -1;
        pthread_mutex_lock(&g_mutex_tasks);
        if (g_proximo_bloco < a->total_blocos) {
            id = g_proximo_bloco++;
        }
        pthread_mutex_unlock(&g_mutex_tasks);

        if (id == -1) break;

        int by = id / a->n_blocos_x;
        int bx = id % a->n_blocos_x;
        processar_bloco(a->img, bx, by, a->bloco_w, a->bloco_h, &local);
    }

    pthread_mutex_lock(&g_mutex_global);
    g_contador_global += local;
    pthread_mutex_unlock(&g_mutex_global);

    return NULL;
}

static void versao_c(Image *img, int n_threads, int bw, int bh) {
    int n_blocos_x = (img->width  + bw - 1) / bw;
    int n_blocos_y = (img->height + bh - 1) / bh;
    int total_blocos = n_blocos_x * n_blocos_y;

    pthread_t *ths = (pthread_t *)malloc((size_t)n_threads * sizeof(pthread_t));
    ThreadArgsC *args = (ThreadArgsC *)malloc((size_t)n_threads * sizeof(ThreadArgsC));

    g_proximo_bloco = 0;

    for (int i = 0; i < n_threads; i++) {
        args[i].thread_id = i;
        args[i].img = img;
        args[i].total_blocos = total_blocos;
        args[i].bloco_w = bw;
        args[i].bloco_h = bh;
        args[i].n_blocos_x = n_blocos_x;
        pthread_create(&ths[i], NULL, thread_func_c, &args[i]);
    }

    for (int i = 0; i < n_threads; i++) pthread_join(ths[i], NULL);
    free(ths); free(args);
}

/* ───────────────────────────────────────────────
 * MAIN – parâmetros:
 * ./image_threads [threads] [versao 1|2|3] [bloco_w] [bloco_h] [width] [height]
 * ─────────────────────────────────────────────── */
int main(int argc, char **argv) {
    srand(1234); // fixo p/ comparar resultados

    int n_threads = 4;
    int versao = VERSAO_A;
    int bw = 64, bh = 64;
    int W = 1024, H = 768;

    if (argc > 1) { int v = atoi(argv[1]); if (v > 0) n_threads = v; }
    if (argc > 2) { int v = atoi(argv[2]); if (v >= 1 && v <= 3) versao = v; }
    if (argc > 3) { int v = atoi(argv[3]); if (v > 0) bw = v; }
    if (argc > 4) { int v = atoi(argv[4]); if (v > 0) bh = v; }
    if (argc > 5) { int v = atoi(argv[5]); if (v > 0) W = v; }
    if (argc > 6) { int v = atoi(argv[6]); if (v > 0) H = v; }

    Image *img = carregar_imagem_fake(W, H);
    g_contador_global = 0;

    printf("Imagem %dx%d | threads=%d | versao=%d", W, H, n_threads, versao);
    if (versao != VERSAO_A) printf(" | bloco=%dx%d", bw, bh);
    printf("\n");

    switch (versao) {
        case VERSAO_A:
            versao_a(img, n_threads);
            break;
        case VERSAO_B:
            versao_b(img, n_threads, bw, bh);
            break;
        case VERSAO_C:
            versao_c(img, n_threads, bw, bh);
            break;
        default:
            fprintf(stderr, "Versao invalida.\n");
            break;
    }

    printf("Contador final = %lld\n", g_contador_global);

    free(img->data);
    free(img);
    return 0;
}
