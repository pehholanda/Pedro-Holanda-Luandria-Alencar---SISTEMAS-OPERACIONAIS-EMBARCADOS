// monitor.c

#include "produtor_consumidor.h"

int main() {
    mqd_t mqd_log_monitor;
    char buffer[MQ_MSG_SIZE];
    ssize_t bytes_read;
    
    // Configuração para abrir a fila de mensagens (apenas leitura)
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // O_CREAT não é usado aqui, pois a fila deve ser criada pelo processo principal (gerador_e_estacao.c)
    mqd_log_monitor = mq_open(MQ_NAME, O_RDONLY, 0666, &attr);
    if (mqd_log_monitor == (mqd_t)-1) {
        perror("mq_open (monitor)");
        fprintf(stderr, "Certifique-se de que 'gerador_e_estacao' esteja rodando primeiro para criar a fila.\n");
        exit(EXIT_FAILURE);
    }
    
    printf("--- Painel de Controle (Monitor) Iniciado. Aguardando logs... ---\n");
    
    while (1) {
        // Bloqueia e espera por uma nova mensagem
        bytes_read = mq_receive(mqd_log_monitor, buffer, MQ_MSG_SIZE, NULL);
        
        if (bytes_read == -1) {
            perror("mq_receive");
            break;
        }
        
        // Garante que a string seja terminada em nulo
        buffer[bytes_read] = '\0';
        
        // Exibe a mensagem de log
        printf(">>> %s\n", buffer);
    }
    
    mq_close(mqd_log_monitor);
    return 0;
}
