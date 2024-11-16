#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

#define BUFF_SIZE 5
#define NUM_FILES 50
#define MATRIX_SIZE 10

// Estrutura S conforme especificada no exercício
typedef struct {
    char filename[100];        // Nome do arquivo
    double A[MATRIX_SIZE][MATRIX_SIZE]; // Matriz A
    double B[MATRIX_SIZE][MATRIX_SIZE]; // Matriz B
    double C[MATRIX_SIZE][MATRIX_SIZE]; // Matriz C (resultado A*B)
    double V[MATRIX_SIZE];              // Vetor (soma das colunas de C)
    double E;                           // Soma dos elementos de V
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

SharedBuffer shared[4];         // Buffers compartilhados

// Inicializa buffers compartilhados
void init_shared_buffers() {
    for (int i = 0; i < 4; i++) {
        shared[i].in = 0;
        shared[i].out = 0;
        sem_init(&shared[i].full, 0, 0);           // Inicialmente vazio
        sem_init(&shared[i].empty, 0, BUFF_SIZE); // Todos os slots estão disponíveis
        sem_init(&shared[i].mutex, 0, 1);         // Mutex começa desbloqueado
    }
}

// Função para gerar números aleatórios double
double random_double(double min, double max) {
    return min + (rand() / (double)RAND_MAX) * (max - min);
}

// Função para criar arquivos de entrada
void create_input_files() {
    FILE *list_file = fopen("entrada.in", "w");
    if (!list_file) {
        perror("Erro ao criar o arquivo entrada.in");
        exit(EXIT_FAILURE);
    }

    for (int file_index = 0; file_index < NUM_FILES; file_index++) {
        char filename[20];
        snprintf(filename, sizeof(filename), "input_%d.txt", file_index);

        // Escreve o nome do arquivo na lista de entrada
        fprintf(list_file, "%s\n", filename);

        FILE *input_file = fopen(filename, "w");
        if (!input_file) {
            perror("Erro ao criar arquivo de entrada");
            exit(EXIT_FAILURE);
        }

        // Gera e escreve a matriz A
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                double value = random_double(1.0, 10.0); // Valores entre 1.0 e 10.0
                fprintf(input_file, "%.2lf%c", value, (j == MATRIX_SIZE - 1) ? '\n' : ' ');
            }
        }

        fprintf(input_file, "\n"); // Separador entre A e B

        // Gera e escreve a matriz B
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                double value = random_double(1.0, 10.0);
                fprintf(input_file, "%.2lf%c", value, (j == MATRIX_SIZE - 1) ? '\n' : ' ');
            }
        }

        fclose(input_file);
        printf("Arquivo %s criado com sucesso.\n", filename);
    }

    fclose(list_file);
}

// Função principal para integrar tudo
int main() {
    // Inicializar gerador de números aleatórios
    srand(time(NULL));

    // Criar arquivos de entrada
    create_input_files();

    printf("Arquivos de entrada criados com separadores de espaço.\n");
    return 0;
}