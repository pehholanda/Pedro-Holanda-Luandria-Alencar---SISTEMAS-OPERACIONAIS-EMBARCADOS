# Pedro-Holanda-Luandria-Alencar---SISTEMAS-OPERACIONAIS-DE-TEMPO-REAL

#sISTEMAS OPERACIONAIS DE TEMPO REAL

AMBIENTE: 
máquina virtual Ubuntu

Compilador GCC.

Terminal Linux.

OBJETIVO:

simulação de um sistema de monitoramento e controle de linha de produção usando as ferramentas de concorrência e sincronização do Linux/Ubuntu, configurado com o kernel PREEMPT_RT

COMO EXECUTAR:
Baixar e adicionar os 3 arquivos no ubuntu

produtor_consumidor.h
gerador_e_estacao.c
monitor.c


compilar e executar nessa sequência

abrir 1 terminal:
 gcc gerador_e_estacao.c -o gerador_e_estacao -lpthread -lrt

 gcc monitor.c -o monitor -lrt

 ./gerador_e_estacao

 em um segundo terminal:

 ./monitor
