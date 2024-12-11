#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/daemon_control_fifo"
#define MAX_THREADS 10

void print_menu() {
    printf("\nMenu de Controle do Daemon:\n");
    printf("1. Alterar número de threads produtoras\n");
    printf("2. Alterar número de threads CP1\n");
    printf("3. Alterar número de threads CP2\n");
    printf("4. Alterar número de threads CP3\n");
    printf("5. Alterar número de threads consumidoras\n");
    printf("6. Enviar SIGTERM para o daemon\n");
    printf("7. Sair\n");
    printf("Escolha uma opção: ");
}

void send_command(const char *command) {
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Erro ao abrir FIFO");
        exit(EXIT_FAILURE);
    }

    write(fd, command, strlen(command));
    close(fd);
}

int main() {
    int option;
    int num_threads;
    char command[256];

    printf("Programa de Controle do Daemon\n");
    printf("Aguardando o daemon iniciar...\n");

    if (access(FIFO_PATH, F_OK) == -1) {
        perror("FIFO não encontrado. Certifique-se de que o daemon está em execução.");
        exit(EXIT_FAILURE);
    }

    while (1) {
        print_menu();
        scanf("%d", &option);

        switch (option) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                printf("Digite o número de threads desejado (1-%d): ", MAX_THREADS);
                scanf("%d", &num_threads);

                if (num_threads < 1 || num_threads > MAX_THREADS) {
                    printf("Número inválido. O máximo permitido é %d.\n", MAX_THREADS);
                    break;
                }

                snprintf(command, sizeof(command), "SET_THREADS %d %d\n", option, num_threads);
                send_command(command);
                printf("Comando enviado: %s", command);
                break;

            case 6:
                printf("Enviando SIGTERM para o daemon...\n");
                snprintf(command, sizeof(command), "SHUTDOWN\n");
                send_command(command);
                break;

            case 7:
                printf("Saindo...\n");
                return 0;

            default:
                printf("Opção inválida.\n");
        }
    }

    return 0;
}
