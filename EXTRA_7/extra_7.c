#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFF_SIZE 5
#define MATRIX_SIZE 10

// Estrutura S para armazenar os dados processados
typedef struct {
    char filename[256];
    char filepath[256];
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

SharedBuffer shared;
volatile int items_remaining = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
int stop_processing = 0; // Flag para controle de término

char input_dir[256];
char output_dir[256];
int resume_mode = 0; // 0: normal, 1: resume

// Inicializa o buffer compartilhado
void init_shared_buffer() {
    shared.in = 0;
    shared.out = 0;
    sem_init(&shared.full, 0, 0);
    sem_init(&shared.empty, 0, BUFF_SIZE);
    sem_init(&shared.mutex, 0, 1);
}

// Renomeia o arquivo alterando sua extensão
void rename_file_extension(const char *filepath, const char *new_ext, char *new_filepath) {
    strcpy(new_filepath, filepath);
    char *dot = strrchr(new_filepath, '.');
    if (dot) {
        strcpy(dot, new_ext);
    } else {
        strcat(new_filepath, new_ext);
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
    while (!stop_processing) {
        DIR *dir = opendir(input_dir);
        if (!dir) {
            perror("Erro ao abrir diretório de entrada");
            pthread_exit(NULL);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".ready") != NULL) {
                S *data = malloc(sizeof(S));

                snprintf(data->filename, sizeof(data->filename), "%s", entry->d_name);
                snprintf(data->filepath, sizeof(data->filepath), "%s/%s", input_dir, entry->d_name);

                // Renomeia para .processing
                char new_filepath[256];
                rename_file_extension(data->filepath, ".processing", new_filepath);
                rename(data->filepath, new_filepath);
                strcpy(data->filepath, new_filepath);

                FILE *file = fopen(data->filepath, "r");
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

                sem_wait(&shared.empty);
                sem_wait(&shared.mutex);

                shared.buffer[shared.in] = data;
                shared.in = (shared.in + 1) % BUFF_SIZE;

                pthread_mutex_lock(&count_mutex);
                items_remaining++;
                pthread_mutex_unlock(&count_mutex);

                sem_post(&shared.mutex);
                sem_post(&shared.full);

                printf("P adicionou: %s\n", data->filename);
            }
        }
        closedir(dir);
        sleep(1); // Evita polling contínuo
    }
    pthread_exit(NULL);
}

// Thread consumidora (C)
void *consumer(void *arg) {
    while (items_remaining > 0 || !stop_processing) {
        sem_wait(&shared.full);
        sem_wait(&shared.mutex);

        if (items_remaining == 0 && stop_processing) {
            sem_post(&shared.mutex);
            break;
        }

        S *data = shared.buffer[shared.out];
        shared.out = (shared.out + 1) % BUFF_SIZE;

        pthread_mutex_lock(&count_mutex);
        items_remaining--;
        pthread_mutex_unlock(&count_mutex);

        sem_post(&shared.mutex);
        sem_post(&shared.empty);

        multiply_matrices(data->A, data->B, data->C);
        calculate_column_sums(data->C, data->V);
        data->E = calculate_vector_sum(data->V);

        // Renomeia para .done
        char done_filepath[256];
        rename_file_extension(data->filepath, ".done", done_filepath);
        rename(data->filepath, done_filepath);

        // Gera arquivo de saída
        char output_filepath[256];
        snprintf(output_filepath, sizeof(output_filepath), "%s/%s.out", output_dir, data->filename);
        rename_file_extension(output_filepath, "", output_filepath);

        FILE *output = fopen(output_filepath, "w");
        if (!output) {
            perror("Erro ao abrir arquivo de saída");
            free(data);
            continue;
        }

        fprintf(output, "E: %.2lf\n", data->E);
        fclose(output);

        printf("C processou e salvou: %s\n", data->filename);
        free(data);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <diretorio_entrada> <diretorio_saida> [-resume]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(input_dir, argv[1]);
    strcpy(output_dir, argv[2]);
    if (argc == 4 && strcmp(argv[3], "-resume") == 0) {
        resume_mode = 1;
    }

    pthread_t producer_thread, consumer_thread;

    init_shared_buffer();

    pthread_create(&producer_thread, NULL, producer, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    printf("Execução concluída.\n");
    return 0;
}
