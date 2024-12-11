#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_FILES 5
#define MATRIX_SIZE 10

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

int main() {
    // Inicializar gerador de números aleatórios
    srand(time(NULL));

    // Criar 10 arquivos de entrada
    create_input_files();

    printf("10 arquivos de entrada criados com separadores de espaço.\n");
    return 0;
}