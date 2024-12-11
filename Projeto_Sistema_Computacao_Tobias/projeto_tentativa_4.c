// Cabeçalhos e Variáveis Globais

#define NUM_CP1 5
#define NUM_CP2 4
#define NUM_CP3 3

Shared shared[4];

void *P(void *arg);
void *CP1(void *arg);
void *CP2(void *arg);
void *CP3(void *arg);
void *C(void *arg);

//Inicialização do SHARED

void init_shared() {
    for (int i = 0; i < 4; i++) {
        shared[i].in = 0;
        shared[i].out = 0;
        sem_init(&shared[i].full, 0, 0);
        sem_init(&shared[i].empty, 0, BUFFER_SIZE);
        sem_init(&shared[i].mutex, 0, 1);
    }
}

// Funções das Threads

void *P(void *arg) {
    FILE *file = fopen("entrada.in", "r");
    if (!file) {
        perror("Erro ao abrir entrada.in");
        exit(EXIT_FAILURE);
    }

    char filename[100];
    while (fgets(filename, sizeof(filename), file)) {
        S *item = (S *)malloc(sizeof(S));
        sscanf(filename, "%s", item->filename);

        // Lê as matrizes A e B do arquivo
        FILE *input = fopen(item->filename, "r");
        if (!input) {
            perror("Erro ao abrir arquivo de entrada");
            free(item);
            continue;
        }

        for (int i = 0; i < MATRIX_SIZE; i++)
            for (int j = 0; j < MATRIX_SIZE; j++)
                fscanf(input, "%lf", &item->A[i][j]);

        for (int i = 0; i < MATRIX_SIZE; i++)
            for (int j = 0; j < MATRIX_SIZE; j++)
                fscanf(input, "%lf", &item->B[i][j]);

        fclose(input);

        // Insere no buffer shared[0]
        sem_wait(&shared[0].empty);
        sem_wait(&shared[0].mutex);

        shared[0].buffer[shared[0].in] = item;
        shared[0].in = (shared[0].in + 1) % BUFFER_SIZE;

        sem_post(&shared[0].mutex);
        sem_post(&shared[0].full);
    }

    fclose(file);
    return NULL;
}

int main() {
    pthread_t prod, cp1[NUM_CP1], cp2[NUM_CP2], cp3[NUM_CP3], cons;

    init_shared();

    pthread_create(&prod, NULL, P, NULL);

    for (int i = 0; i < NUM_CP1; i++)
        pthread_create(&cp1[i], NULL, CP1, NULL);

    for (int i = 0; i < NUM_CP2; i++)
        pthread_create(&cp2[i], NULL, CP2, NULL);

    for (int i = 0; i < NUM_CP3; i++)
        pthread_create(&cp3[i], NULL, CP3, NULL);

    pthread_create(&cons, NULL, C, NULL);

    pthread_join(cons, NULL);

    return 0;
}