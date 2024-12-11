// é necessário habilitar OpenMP no Compilador: Certifique-se de compilar o código com suporte ao OpenMP,
// exemplo: gcc -pthread -fopenmp -o extra_3 extra_3.c


//OpenMP para CP1

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <omp.h>

#define BUFF_SIZE 5
#define MATRIX_SIZE 10
#define NUM_FILES 10

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
volatile int items_remaining[4] = {0, 0, 0, 0}; // Rastreia itens nos buffers
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
int stop_processing = 0; // Flag para controle de término

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

// Função para multiplicar matrizes usando OpenMP
void multiply_matrices(double A[MATRIX_SIZE][MATRIX_SIZE], double B[MATRIX_SIZE][MATRIX_SIZE], double C[MATRIX_SIZE][MATRIX_SIZE]) {
    #pragma omp parallel for num_threads(2)
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

        pthread_mutex_lock(&count_mutex);
        items_remaining[0]++;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].full);

        printf("P adicionou: %s\n", data->filename);
    }

    fclose(input);
    stop_processing = 1; // Indica que o produtor terminou
    pthread_exit(NULL);
}

// Thread consumidora-produtora (CP1)
void *consumer_producer1(void *arg) {
    while (items_remaining[0] > 0 || stop_processing == 0) {
        sem_wait(&shared[0].full);
        sem_wait(&shared[0].mutex);

        if (items_remaining[0] == 0 && stop_processing) {
            sem_post(&shared[0].mutex);
            break;
        }

        S *data = shared[0].buffer[shared[0].out];
        shared[0].out = (shared[0].out + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[0]--;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].empty);

        // Multiplica matrizes em paralelo
        multiply_matrices(data->A, data->B, data->C);

        sem_wait(&shared[1].empty);
        sem_wait(&shared[1].mutex);

        shared[1].buffer[shared[1].in] = data;
        shared[1].in = (shared[1].in + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[1]++;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[1].mutex);
        sem_post(&shared[1].full);

        printf("CP1 processou: %s\n", data->filename);
    }
    pthread_exit(NULL);
}

// Thread consumidora-produtora (CP2)
void *consumer_producer2(void *arg) {
    while (items_remaining[1] > 0 || stop_processing == 0) {
        sem_wait(&shared[1].full);
        sem_wait(&shared[1].mutex);

        if (items_remaining[1] == 0 && stop_processing) {
            sem_post(&shared[1].mutex);
            break;
        }

        S *data = shared[1].buffer[shared[1].out];
        shared[1].out = (shared[1].out + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[1]--;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[1].mutex);
        sem_post(&shared[1].empty);

        calculate_column_sums(data->C, data->V);

        sem_wait(&shared[2].empty);
        sem_wait(&shared[2].mutex);

        shared[2].buffer[shared[2].in] = data;
        shared[2].in = (shared[2].in + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[2]++;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[2].mutex);
        sem_post(&shared[2].full);

        printf("CP2 processou: %s\n", data->filename);
    }
    pthread_exit(NULL);
}

// Thread consumidora-produtora (CP3)
void *consumer_producer3(void *arg) {
    while (items_remaining[2] > 0 || stop_processing == 0) {
        sem_wait(&shared[2].full);
        sem_wait(&shared[2].mutex);

        if (items_remaining[2] == 0 && stop_processing) {
            sem_post(&shared[2].mutex);
            break;
        }

        S *data = shared[2].buffer[shared[2].out];
        shared[2].out = (shared[2].out + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[2]--;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[2].mutex);
        sem_post(&shared[2].empty);

        data->E = calculate_vector_sum(data->V);

        sem_wait(&shared[3].empty);
        sem_wait(&shared[3].mutex);

        shared[3].buffer[shared[3].in] = data;
        shared[3].in = (shared[3].in + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[3]++;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[3].mutex);
        sem_post(&shared[3].full);

        printf("CP3 processou: %s\n", data->filename);
    }
    pthread_exit(NULL);
}

// Thread consumidora (C)
void *consumer(void *arg) {
    int processed_count = 0;
    while (processed_count < NUM_FILES) {
        sem_wait(&shared[3].full);
        sem_wait(&shared[3].mutex);

        S *data = shared[3].buffer[shared[3].out];
        shared[3].out = (shared[3].out + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining[3]--;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared[3].mutex);
        sem_post(&shared[3].empty);

        FILE *output = fopen("saida.out", "a");
        if (!output) {
            perror("Erro ao abrir saida.out");
            free(data);
            continue;
        }

        fprintf(output, "================================\n");
        fprintf(output, "Entrada: %s\n", data->filename);

        fprintf(output, "Matriz A:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf ", data->A[i][j]);
            }
            fprintf(output, "\n");
        }

        fprintf(output, "Matriz B:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf ", data->B[i][j]);
            }
            fprintf(output, "\n");
        }

        fprintf(output, "Matriz C:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf ", data->C[i][j]);
            }
            fprintf(output, "\n");
        }

        fprintf(output, "Vetor V:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            fprintf(output, "%.2lf\n", data->V[i]);
        }

        fprintf(output, "E: %.2lf\n", data->E);
        fprintf(output, "================================\n");

        fclose(output);

        printf("C processou e salvou: %s\n", data->filename);

        free(data);
        processed_count++;
    }

    pthread_exit(NULL);
}

int main() {
    pthread_t producer_thread, cp1_thread, cp2_thread, cp3_thread, consumer_thread;

    init_shared_buffers();

    pthread_create(&producer_thread, NULL, producer, NULL);
    pthread_create(&cp1_thread, NULL, consumer_producer1, NULL);
    pthread_create(&cp2_thread, NULL, consumer_producer2, NULL);
    pthread_create(&cp3_thread, NULL, consumer_producer3, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    pthread_join(producer_thread, NULL);
    pthread_join(cp1_thread, NULL);
    pthread_join(cp2_thread, NULL);
    pthread_join(cp3_thread, NULL);
    pthread_join(consumer_thread, NULL);

    printf("Execução concluída.\n");
    return 0;
}
