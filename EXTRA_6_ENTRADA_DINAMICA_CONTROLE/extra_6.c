#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#define BUFF_SIZE 5
#define MATRIX_SIZE 10

// Estrutura S para armazenar os dados processados
typedef struct {
    char filename[100];
    double A[MATRIX_SIZE][MATRIX_SIZE];
    double B[MATRIX_SIZE][MATRIX_SIZE];
    double C[MATRIX_SIZE][MATRIX_SIZE];
    double V[MATRIX_SIZE];
    double E;
} S;

// Buffer compartilhado
typedef struct {
    S *buffer[BUFF_SIZE];
    int in;
    int out;
    sem_t full;
    sem_t empty;
    sem_t mutex;
} SharedBuffer;

SharedBuffer shared[4];
volatile int stop[4] = {0, 0, 0, 0}; // Controle de término distribuído
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

// Inicializa buffers compartilhados
void init_shared_buffers() {
    for (int i = 0; i < 4; i++) {
        shared[i].in = 0;
        shared[i].out = 0;
        sem_init(&shared[i].full, 0, 0);
        sem_init(&shared[i].empty, 0, BUFF_SIZE);
        sem_init(&shared[i].mutex, 0, 1);
    }
}

// Função para multiplicar matrizes
void multiply_matrices(double A[MATRIX_SIZE][MATRIX_SIZE], double B[MATRIX_SIZE][MATRIX_SIZE], double C[MATRIX_SIZE][MATRIX_SIZE]) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            C[i][j] = 0;
            for (int k = 0; k < MATRIX_SIZE; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Função para calcular a soma das colunas da matriz C
void calculate_column_sums(double C[MATRIX_SIZE][MATRIX_SIZE], double V[MATRIX_SIZE]) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
        V[j] = 0;
        for (int i = 0; i < MATRIX_SIZE; i++) {
            V[j] += C[i][j];
        }
    }
}

// Função para calcular a soma dos elementos do vetor V
double calculate_vector_sum(double V[MATRIX_SIZE]) {
    double sum = 0;
    for (int i = 0; i < MATRIX_SIZE; i++) {
        sum += V[i];
    }
    return sum;
}

// Thread produtora (P)
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

        FILE *file = fopen(data->filename, "r");
        if (!file) {
            perror("Erro ao abrir arquivo de entrada");
            free(data);
            continue;
        }

        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fscanf(file, "%lf", &data->A[i][j]);
            }
        }

        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fscanf(file, "%lf", &data->B[i][j]);
            }
        }

        fclose(file);

        sem_wait(&shared[0].empty);
        sem_wait(&shared[0].mutex);

        shared[0].buffer[shared[0].in] = data;
        shared[0].in = (shared[0].in + 1) % BUFF_SIZE;

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].full);
    }

    fclose(input);
    stop[0] = 1; // Sinaliza que o produtor terminou
    pthread_exit(NULL);
}

// Thread consumidora-produtora genérica
void *consumer_producer(void *arg) {
    int index = *(int *)arg;

    while (!stop[index] || sem_trywait(&shared[index].full) == 0) {
        sem_wait(&shared[index].mutex);
        if (!stop[index] && shared[index].buffer[shared[index].out] == NULL) {
            sem_post(&shared[index].mutex);
            continue;
        }

        S *data = shared[index].buffer[shared[index].out];
        shared[index].out = (shared[index].out + 1) % BUFF_SIZE;

        sem_post(&shared[index].mutex);
        sem_post(&shared[index].empty);

        if (index == 0) {
            multiply_matrices(data->A, data->B, data->C);
        } else if (index == 1) {
            calculate_column_sums(data->C, data->V);
        } else if (index == 2) {
            data->E = calculate_vector_sum(data->V);
        }

        sem_wait(&shared[index + 1].empty);
        sem_wait(&shared[index + 1].mutex);

        shared[index + 1].buffer[shared[index + 1].in] = data;
        shared[index + 1].in = (shared[index + 1].in + 1) % BUFF_SIZE;

        sem_post(&shared[index + 1].mutex);
        sem_post(&shared[index + 1].full);
    }

    stop[index + 1] = 1; // Sinaliza que a próxima etapa pode terminar
    pthread_exit(NULL);
}

// Thread consumidora final
void *consumer(void *arg) {
    while (!stop[3] || sem_trywait(&shared[3].full) == 0) {
        sem_wait(&shared[3].mutex);

        S *data = shared[3].buffer[shared[3].out];
        shared[3].out = (shared[3].out + 1) % BUFF_SIZE;

        sem_post(&shared[3].mutex);
        sem_post(&shared[3].empty);

        FILE *output = fopen("saida.out", "a");
        if (!output) {
            perror("Erro ao abrir saida.out");
            free(data);
            continue;
        }

        fprintf(output, "Entrada: %s\n", data->filename);
        fprintf(output, "E: %.2lf\n", data->E);

        fclose(output);
        free(data);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t producer_thread, cp_threads[3], consumer_thread;
    int indices[3] = {0, 1, 2};

    init_shared_buffers();

    pthread_create(&producer_thread, NULL, producer, NULL);

    for (int i = 0; i < 3; i++) {
        pthread_create(&cp_threads[i], NULL, consumer_producer, &indices[i]);
    }

    pthread_create(&consumer_thread, NULL, consumer, NULL);

    pthread_join(producer_thread, NULL);
    for (int i = 0; i < 3; i++) {
        pthread_join(cp_threads[i], NULL);
    }
    pthread_join(consumer_thread, NULL);

    printf("Execução concluída.\n");
    return 0;
}
