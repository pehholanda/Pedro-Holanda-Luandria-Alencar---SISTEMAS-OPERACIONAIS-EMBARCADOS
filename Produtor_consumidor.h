#ifndef PRODUTOR_CONSUMIDOR_H
#define PRODUTOR_CONSUMIDOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>

// --- Definições Globais ---
#define NUM_CONSUMIDORES 3     // Número de Threads Consumidoras na Estação
#define TAMANHO_BUFFER 5       // Capacidade máxima do buffer compartilhado
#define SHM_NAME "/shm_buffer_rtos" // Nome para a Memória Compartilhada
#define SEM_MUTEX_NAME "/sem_mutex_rtos" // Nome para o Mutex (Semáforo)
#define SEM_FULL_NAME "/sem_full_rtos"   // Semáforo para contar itens (cheios)
#define SEM_EMPTY_NAME "/sem_empty_rtos" // Semáforo para contar espaços (vazios)
#define MQ_NAME "/mq_log_rtos"       // Nome da Fila de Mensagens (Log)
#define MQ_MAX_MSGS 10
#define MQ_MSG_SIZE 256 // Tamanho máximo da mensagem de log

// --- Estrutura de Dados ---
typedef struct {
    int id_peca;
    int qualidade; // 0 = Ruim, 1 = Boa
    // Adicionar outros campos aqui para simulação mais complexa
} Pecas;

// --- Estrutura do Buffer Compartilhado ---
typedef struct {
    Pecas buffer[TAMANHO_BUFFER];
    int entrada; // Índice para inserção (Produtor)
    int saida;   // Índice para remoção (Consumidor)
} BufferCompartilhado;

// --- Variáveis Globais de IPC (para serem usadas após o link) ---
extern BufferCompartilhado *shm_buffer;
extern sem_t *sem_mutex;   // Proteção da Seção Crítica (Exclusão Mútua)
extern sem_t *sem_full;    // Sincronização: conta itens no buffer (Full)
extern sem_t *sem_empty;   // Sincronização: conta espaços no buffer (Empty)
extern mqd_t mqd_log;      // Descritor da Fila de Mensagens

#endif // PRODUTOR_CONSUMIDOR_H
