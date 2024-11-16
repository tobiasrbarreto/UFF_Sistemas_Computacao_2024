#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#define BUFF_SIZE 5
#define NUM_FILES 50

typedef struct {
    char filename[100];        // Nome do arquivo
    double A[10][10];          // Matriz A
    double B[10][10];          // Matriz B
    double C[10][10];          // Matriz C (resultado A*B)
    double V[10];              // Vetor (soma das colunas de C)
    double E;                  // Soma dos elementos de V
} S;

// Estrutura de buffer compartilhado
typedef struct {
    S *buffer[BUFF_SIZE];      // Buffer com ponteiros para S
    int in;                    // Índice de entrada
    int out;                   // Índice de saída
    sem_t full;                // Slots ocupados
    sem_t empty;               // Slots vazios
    sem_t mutex;               // Controle de acesso
} SharedBuffer;

SharedBuffer shared[4];         // Buffers compartilhados (shared[0] a shared[3])

// Função para inicializar buffers
void init_shared_buffers() {
    for (int i = 0; i < 4; i++) {
        shared[i].in = 0;
        shared[i].out = 0;
        sem_init(&shared[i].full, 0, 0);           // Inicialmente vazio
        sem_init(&shared[i].empty, 0, BUFF_SIZE); // Todos os slots estão disponíveis
        sem_init(&shared[i].mutex, 0, 1);         // Mutex começa desbloqueado
    }
}

// Multiplicação de matrizes
void multiply_matrices(double A[10][10], double B[10][10], double C[10][10]) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            C[i][j] = 0;
            for (int k = 0; k < 10; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Função Produtora (Thread P)
void *producer(void *arg) {
    FILE *input = fopen("entrada.in", "r");
    if (!input) {
        perror("Erro ao abrir entrada.in");
        pthread_exit(NULL);
    }

    char filename[100];
    while (fscanf(input, "%s", filename) != EOF) {
        S *data = malloc(sizeof(S));
        snprintf(data->filename, 100, "%s", filename);

        // Leitura dos arquivos (simplificada, pois depende de formato específico)
        FILE *file = fopen(data->filename, "r");
        if (!file) {
            perror("Erro ao abrir arquivo de entrada");
            free(data);
            continue;
        }

        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                fscanf(file, "%lf", &data->A[i][j]);
            }
        }

        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                fscanf(file, "%lf", &data->B[i][j]);
            }
        }
        fclose(file);

        // Inserir no buffer compartilhado
        sem_wait(&shared[0].empty);
        sem_wait(&shared[0].mutex);

        shared[0].buffer[shared[0].in] = data;
        shared[0].in = (shared[0].in + 1) % BUFF_SIZE;

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].full);

        printf("Produtor adicionou: %s\n", data->filename);
    }

    fclose(input);
    pthread_exit(NULL);
}

// Função Consumidora-Produtora CP1
void *consumer_producer1(void *arg) {
    while (1) {
        sem_wait(&shared[0].full);
        sem_wait(&shared[0].mutex);

        S *data = shared[0].buffer[shared[0].out];
        shared[0].out = (shared[0].out + 1) % BUFF_SIZE;

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].empty);

        // Multiplicar A e B
        multiply_matrices(data->A, data->B, data->C);

        sem_wait(&shared[1].empty);
        sem_wait(&shared[1].mutex);

        shared[1].buffer[shared[1].in] = data;
        shared[1].in = (shared[1].in + 1) % BUFF_SIZE;

        sem_post(&shared[1].mutex);
        sem_post(&shared[1].full);

        printf("CP1 processou: %s\n", data->filename);
    }
    pthread_exit(NULL);
}

// Outras funções (CP2, CP3, C) podem ser implementadas de forma similar.

int main() {
    pthread_t producer_thread, cp1_thread;

    // Inicializar buffers compartilhados
    init_shared_buffers();

    // Criar threads
    pthread_create(&producer_thread, NULL, producer, NULL);
    pthread_create(&cp1_thread, NULL, consumer_producer1, NULL);

    // Aguardar threads
    pthread_join(producer_thread, NULL);
    pthread_join(cp1_thread, NULL);

    return 0;
}
