#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>

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
volatile int items_remaining[4] = {0, 0, 0, 0};
volatile int stop_processing = 0; // Para SIGTERM
volatile int force_stop = 0;      // Para SIGKILL
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

// Signal handlers
void handle_sigterm(int sig) {
    syslog(LOG_INFO, "SIGTERM recebido. Finalizando processamento após concluir tarefas.");
    stop_processing = 1;
}

void handle_sigkill(int sig) {
    syslog(LOG_ERR, "SIGKILL recebido. Encerrando imediatamente.");
    force_stop = 1;
    exit(1);
}

// Transformar em daemon
void daemonize() {
    pid_t pid;

    // Criar processo filho
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Terminar processo pai

    // Tornar-se líder de sessão
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Criar segundo filho para evitar reaquisição do terminal
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // Redirecionar stdin, stdout, stderr para /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    // Alterar máscara de arquivo
    umask(0);

    // Mudar diretório para root
    chdir("/");
}

// Funções produtoras e consumidoras
void *producer(void *arg) {
    syslog(LOG_INFO, "Produtor iniciado.");
    FILE *input = fopen("entrada.in", "r");
    if (!input) {
        syslog(LOG_ERR, "Erro ao abrir entrada.in");
        pthread_exit(NULL);
    }

    char filename[100];
    while (fscanf(input, "%s", filename) != EOF && !force_stop) {
        S *data = malloc(sizeof(S));
        snprintf(data->filename, 100, "%s", filename);

        FILE *file = fopen(data->filename, "r");
        if (!file) {
            syslog(LOG_ERR, "Erro ao abrir arquivo %s", filename);
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

        syslog(LOG_INFO, "Produtor adicionou: %s", data->filename);
    }

    fclose(input);
    stop_processing = 1; // Indica que o produtor terminou
    syslog(LOG_INFO, "Produtor finalizado.");
    pthread_exit(NULL);
}

void *consumer(void *arg) {
    syslog(LOG_INFO, "Consumidor iniciado.");
    int processed_count = 0;
    while (!force_stop && (processed_count < NUM_FILES || !stop_processing)) {
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
            syslog(LOG_ERR, "Erro ao abrir saida.out");
            free(data);
            continue;
        }

        fprintf(output, "================================\n");
        fprintf(output, "Entrada: %s\n", data->filename);
        fprintf(output, "——————————–\nMatriz A:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf%s", data->A[i][j], (j == MATRIX_SIZE - 1) ? "\n" : " ");
            }
        }
        fprintf(output, "——————————–\nMatriz B:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf%s", data->B[i][j], (j == MATRIX_SIZE - 1) ? "\n" : " ");
            }
        }
        fprintf(output, "——————————–\nMatriz C:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                fprintf(output, "%.2lf%s", data->C[i][j], (j == MATRIX_SIZE - 1) ? "\n" : " ");
            }
        }
        fprintf(output, "——————————–\nVetor V:\n");
        for (int i = 0; i < MATRIX_SIZE; i++) {
            fprintf(output, "%.2lf\n", data->V[i]);
        }
        fprintf(output, "——————————–\nE: %.2lf\n", data->E);
        fprintf(output, "================================\n");

        fclose(output);

        syslog(LOG_INFO, "Consumidor processou e salvou: %s", data->filename);

        free(data);
        processed_count++;
    }
    syslog(LOG_INFO, "Consumidor finalizado.");
    pthread_exit(NULL);
}

// Outras funções consumidoras-produtoras (CP1, CP2, CP3) seguem o mesmo padrão

int main() {
    pthread_t producer_thread, cp1_thread, cp2_thread, cp3_thread, consumer_thread;

    openlog("ProdutorConsumidorDaemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    daemonize();
    syslog(LOG_INFO, "Daemon iniciado.");

    init_shared_buffers();
    signal(SIGTERM, handle_sigterm);
    signal(SIGKILL, handle_sigkill);

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

    syslog(LOG_INFO, "Daemon finalizado.");
    closelog();
    return 0;
}