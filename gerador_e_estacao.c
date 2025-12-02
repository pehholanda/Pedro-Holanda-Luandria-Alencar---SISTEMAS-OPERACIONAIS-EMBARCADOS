// gerador_e_estacao.c

#include "produtor_consumidor.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

// Definição das variáveis globais (definidas extern em header)
BufferCompartilhado *shm_buffer = NULL;
sem_t *sem_mutex = NULL;
sem_t *sem_full = NULL;
sem_t *sem_empty = NULL;
mqd_t mqd_log = (mqd_t)-1;

// --- Protótipos de Funções ---
void inicializar_ipc();
void finalizar_ipc();
void cleanup_old_ipc();
void *tarefa_consumidora(void *thread_id);
void gerador_pecas();

// --- Helpers ---
static void msleep(int ms) { struct timespec ts = { ms/1000, (ms%1000)*1000000L }; nanosleep(&ts, NULL); }

// --- Implementação da Tarefa Consumidora (Estação de Inspeção) ---
void *tarefa_consumidora(void *thread_id) {
    long tid = (long)(intptr_t)thread_id;
    Pecas peca;
    char log_msg[MQ_MSG_SIZE];

    printf("[Estação %ld] Iniciada. Aguardando peças...\n", tid);

    while (1) {
        // 1. Sincronização: Esperar por um item no buffer (sem_full)
        if (sem_wait(sem_full) == -1) { perror("sem_wait sem_full"); break; }

        // 2. Exclusão Mútua: Bloquear o acesso à seção crítica (buffer)
        if (sem_wait(sem_mutex) == -1) { perror("sem_wait sem_mutex"); break; }

        // --- SEÇÃO CRÍTICA (REMOÇÃO) ---
        peca = shm_buffer->buffer[shm_buffer->saida];
        shm_buffer->saida = (shm_buffer->saida + 1) % TAMANHO_BUFFER;

        int current_full = 0;
        sem_getvalue(sem_full, &current_full);
        printf("[Estação %ld] Pegou Peça %d. (Itens no buffer após remoção: %d)\n", tid, peca.id_peca, current_full);
        // --- FIM DA SEÇÃO CRÍTICA ---

        // 3. Exclusão Mútua: Liberar o acesso
        if (sem_post(sem_mutex) == -1) { perror("sem_post sem_mutex"); break; }

        // 4. Sincronização: Sinalizar que há um espaço vazio no buffer (sem_empty)
        if (sem_post(sem_empty) == -1) { perror("sem_post sem_empty"); break; }

        // --- Simulação de Inspeção (Processamento) ---
        usleep((rand() % 500) * 1000); // Simula tempo de processamento (0ms a 500ms)
        peca.qualidade = (rand() % 2); // 0 = Ruim, 1 = Boa

        // --- 5. Comunicação (IPC - Fila de Mensagens) ---
        snprintf(log_msg, MQ_MSG_SIZE, "[LOG] Estacao %ld | Peca %d | Qualidade: %s",
                 tid, peca.id_peca, (peca.qualidade == 1 ? "**BOA**" : "**RUIM**"));

        if (mqd_log != (mqd_t)-1) {
            if (mq_send(mqd_log, log_msg, strlen(log_msg) + 1, 1) == -1) {
                perror("mq_send (log)");
            }
        }
    }

    pthread_exit(NULL);
}

// --- Implementação do Gerador (Produtor) ---
void gerador_pecas() {
    int peca_id = 1;
    char log_msg[MQ_MSG_SIZE];

    printf("[Gerador] Iniciado. Comecando a producao...\n");
    srand((unsigned)time(NULL) ^ getpid());

    while (1) {
        // 1. Sincronizacao: Esperar por um espaco vazio no buffer (sem_empty)
        if (sem_wait(sem_empty) == -1) { perror("sem_wait sem_empty"); break; }

        // 2. Exclusao Mutua: Bloquear o acesso a secao critica (buffer)
        if (sem_wait(sem_mutex) == -1) { perror("sem_wait sem_mutex"); break; }

        // --- SECAO CRITICA (INSERCAO) ---
        Pecas nova_peca;
        nova_peca.id_peca = peca_id;
        nova_peca.qualidade = 0; // Qualidade inicial (sera inspecionada depois)

        shm_buffer->buffer[shm_buffer->entrada] = nova_peca;
        shm_buffer->entrada = (shm_buffer->entrada + 1) % TAMANHO_BUFFER;

        int current_full = 0;
        sem_getvalue(sem_full, &current_full);
        printf("[Gerador] Produziu Peca %d. (Itens no buffer apos insercao: %d)\n", peca_id, current_full + 1);
        peca_id++;
        // --- FIM DA SECAO CRITICA ---

        // 3. Exclusao Mutua: Liberar o acesso
        if (sem_post(sem_mutex) == -1) { perror("sem_post sem_mutex"); break; }

        // 4. Sincronizacao: Sinalizar que ha um item no buffer (sem_full)
        if (sem_post(sem_full) == -1) { perror("sem_post sem_full"); break; }

        // --- Log (Produtor) ---
        snprintf(log_msg, MQ_MSG_SIZE, "[LOG] Gerador | Produziu Peca %d", nova_peca.id_peca);
        if (mqd_log != (mqd_t)-1) {
            if (mq_send(mqd_log, log_msg, strlen(log_msg) + 1, 1) == -1) {
                perror("mq_send (produtor log)");
            }
        }

        usleep(500 * 1000); // Intervalo de produção (500ms)
    }
}

// --- Inicializacao e Limpeza de IPC ---

// Limpa (unlink) IPCs antigos, ignorando erros se nao existirem
void cleanup_old_ipc() {
    mq_unlink(MQ_NAME);
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_EMPTY_NAME);
}

// 1. Inicializa Memoria Compartilhada e Semaforos POSIX
void inicializar_ipc() {
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // a) Fila de Mensagens POSIX (Log)
    mqd_log = mq_open(MQ_NAME, O_CREAT | O_EXCL | O_WRONLY, 0666, &attr);
    if (mqd_log == (mqd_t)-1) {
        if (errno == EEXIST) {
            mqd_log = mq_open(MQ_NAME, O_WRONLY);
            if (mqd_log == (mqd_t)-1) { perror("mq_open existing (log)"); exit(EXIT_FAILURE); }
        } else {
            perror("mq_open (log)");
            exit(EXIT_FAILURE);
        }
    }

    // b) Memoria Compartilhada POSIX
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(EXIT_FAILURE); }
    if (ftruncate(shm_fd, sizeof(BufferCompartilhado)) == -1) { perror("ftruncate"); exit(EXIT_FAILURE); }

    shm_buffer = (BufferCompartilhado *)mmap(NULL, sizeof(BufferCompartilhado), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_buffer == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }

    // Inicializa indices do buffer (somente quando criamos a SHM pela primeira vez seria ideal zerar)
    // Para simplicidade, inicializamos sempre. Em cenário real, sincronizar a inicializacao entre processos.
    shm_buffer->entrada = 0;
    shm_buffer->saida = 0;
    // Opcional: zerar conteudo
    memset(shm_buffer->buffer, 0, sizeof(shm_buffer->buffer));

    // c) Semaforos POSIX Nomeados
    sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_mutex == SEM_FAILED) {
        if (errno == EEXIST) { sem_mutex = sem_open(SEM_MUTEX_NAME, 0); if (sem_mutex == SEM_FAILED) { perror("sem_open mutex existing"); exit(EXIT_FAILURE); } }
        else { perror("sem_open mutex"); exit(EXIT_FAILURE); }
    }

    sem_full = sem_open(SEM_FULL_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (sem_full == SEM_FAILED) {
        if (errno == EEXIST) { sem_full = sem_open(SEM_FULL_NAME, 0); if (sem_full == SEM_FAILED) { perror("sem_open full existing"); exit(EXIT_FAILURE); } }
        else { perror("sem_open full"); exit(EXIT_FAILURE); }
    }

    sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT | O_EXCL, 0666, TAMANHO_BUFFER);
    if (sem_empty == SEM_FAILED) {
        if (errno == EEXIST) { sem_empty = sem_open(SEM_EMPTY_NAME, 0); if (sem_empty == SEM_FAILED) { perror("sem_open empty existing"); exit(EXIT_FAILURE); } }
        else { perror("sem_open empty"); exit(EXIT_FAILURE); }
    }
}

// 2. Limpa e Desvincula todos os recursos de IPC
void finalizar_ipc() {
    // Fecha e desvincula a Fila de Mensagens
    if (mqd_log != (mqd_t)-1) { mq_close(mqd_log); mq_unlink(MQ_NAME); }

    // Desmapeia e desvincula a Memoria Compartilhada
    if (shm_buffer != NULL) { munmap(shm_buffer, sizeof(BufferCompartilhado)); shm_unlink(SHM_NAME); }

    // Fecha e desvincula os Semaforos
    if (sem_mutex != NULL) { sem_close(sem_mutex); sem_unlink(SEM_MUTEX_NAME); }
    if (sem_full != NULL) { sem_close(sem_full); sem_unlink(SEM_FULL_NAME); }
    if (sem_empty != NULL) { sem_close(sem_empty); sem_unlink(SEM_EMPTY_NAME); }
}

// --- Funcao Principal ---
int main(int argc, char *argv[]) {
    // Limpa IPCs antigos (silencioso) para garantir um ambiente limpo
    cleanup_old_ipc();

    printf("--- Inicializando Sistema de Producao (PID: %d) ---\n", getpid());
    inicializar_ipc(); // Cria e inicializa IPCs

    // 1. Criacao do Processo Filho: Estacao de Inspecao (Consumidor)
    pid_t pid_estacao = fork();

    if (pid_estacao < 0) {
        perror("fork estacao");
        finalizar_ipc();
        exit(EXIT_FAILURE);
    }
    else if (pid_estacao == 0) {
        // --- Codigo do Processo Filho: Estacao de Inspecao ---
        printf("\n[Processo Estacao] Iniciado (PID: %d). Criando threads de trabalho...\n", getpid());
        pthread_t threads[NUM_CONSUMIDORES];
        long i;

        for (i = 0; i < NUM_CONSUMIDORES; i++) {
            if (pthread_create(&threads[i], NULL, tarefa_consumidora, (void *)(intptr_t)i) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        }

        // Mantem o processo Estacao rodando (Threads rodarao em loop infinito)
        for (i = 0; i < NUM_CONSUMIDORES; i++) {
            pthread_join(threads[i], NULL);
        }

        // Esta parte nunca sera alcancada, a menos que o loop infinito seja quebrado por sinal
        exit(0);
    }
    else {
        // --- Codigo do Processo Pai: Gerador de Pecas (Produtor) ---
        gerador_pecas();

        // Espera pelo processo Estacao (nao sera alcancado pois 'gerador_pecas' eh loop infinito)
        wait(NULL);

        finalizar_ipc();
        printf("\n--- Sistema Finalizado ---\n");
    }

    return 0;
}
