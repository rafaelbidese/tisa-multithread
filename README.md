UNIVERSIDADE FEDERAL DE SANTA CATARINA
Programa de Pós-Graduação em Automação e Sistemas
Aluno: Rafael Bidese Puhl
Disciplina: Técnicas de Implementação de Sistemas Automatizados

# Trabalho: Sistema multitarefas de controle via rede
O trabalho se trata de realizar o controle de temperatura e nível de um tanque via rede, utilizando um socket. O sistema deve ser implementado utilizando múltiplas POSIX threads utilizando linguagem de programação C em um sistema Linux.

## Características Principais:
- Cada thread com comunicação tem seu próprio socket
- Cravação em arquivo com buffer duplo em formato .csv com UTC de cada amostra
- Controle on-off com histerese
- Uma thread de leitura dos dados de temperatura e outra de dados de nível
- Acesso as variáveis utilizando monitores para leitura e escrita
- Acesso ao teclado para mudar referência (sem mutex no terminal)
- Função de alarme desliga a planta e finaliza o programa
- Thread periódica implementado na forma "make_periodic", passando ponteiro para a função da thread


## Base de código
O código do programa desenvolvido será descrito de uma forma abstrata utilizando algumas notações em C misturadas com descrições informais para facilitar a compreensão.

### Definições:
- Mensagens do socket
- Valores máximos
- Período de repetição das threads

### Estruturas:
**typedef struct socket_info_t**: se a thread possui um socket, carrega as informações do servidor
**typedef struct time_thread_t**: carrega o periodo de execucao, printeiro para a função da thread, e informações sobre socket
**typedef struct tmeas_t**: carrega as informações lidas do servidor relacionadas a temperatura (ti, t, ta)
**typedef struct hmeas_t**: carrega as informações lidas do servidor relacionadas ao nível (no, h)

### Variáveis globais:
Variáveis monitoradas:
**float ni**: fluxo de entrada (variável de controle)
**float q**: calor de entrada (variável de controle)
**struct tmeas**: tem os valores mais recentemente lidos
**struct hmeas**: tem os valores mais recentemente lidos

Variáveis utilizadas no controle:
**float href**: valor de referência para nível
**float tref**: valor de referëncia para temperatura
**float hctrl_hist**: histerese de controle para nível
**float tctrl_hist**: histerese de controle para temperatura

Variáveis utilizadas na escrita em arquivo:
**char[] buffer1[]**: buffer para fazer o log
**char[] buffer2[]**: buffer para fazer o log

Variáveis das threads:
**pthread_mutex_t mtx_hmeas**: protege acesso a variável hmeas
**pthread_mutex_t mtx_tmeas**: protege acesso a variável tmeas
**pthread_mutex_t mtx_alarm**: protege acesso a thread de alarme
**pthread_mutex_t mtx_log**: protege acesso a aos buffers de escrita
**pthread_mutex_t mtx_term**: protege acesso ao terminal
**pthread_mutex_t mtx_ni**: protege acesso a variável ni
**pthread_mutex_t mtx_q**: protege acesso a variável q
**pthread_mutex_t mtx_tref**: protege acesso a variável tref
**pthread_mutex_t mtx_href**: protege acesso a variável href
**pthread_cond_t full_buff**: variável condicional para disparar thread de escrita em arquivo
**pthread_cond_t go_on**: variável condicional para disparar o alarme


### Monitores:
Todas as variáveis são acessadas por meio de monitores, travando o seu respectivo mutex antes do acesso e liberando logo após 

### Inicialização:
- inicializa struct time_thread_t com struct socket_info_t
para todas threads:
- pthread_create(thread, NULL, periodic_thread, struct time_thread_t)
- pthread_join(thread de alarme)

...

void * periodic_thread(void *)

- typecast a struct que chegou
- ve se é uma thread que utiliza socket, se sim, abre socket.
- inicializa timer
while {
se ocorrer o alarme, pthread_exit(NULL)
clock_nanosleep
executa ponteiro de função passando struct socket_info
conta tempo que falta
}

### Ponteiros de funções executado nas threads

**void t_tmeas (void[] arg)**
- cria variáveis locais
- lê os dados pelo socket
- se a variável passou dos limites, dispara o sinal go_on para thread de alarme
- escreve nas variáveis globais

**void t_hmeas (void[] arg)**
- cria variáveis locais
- lê os dados pelo socket
- se a variável passou dos limites, dispara o sinal go_on para thread de alarme
- escreve nas variáveis globais

**void t_logrd (void[] arg)**
- cria variáveis locais
- cria string com tempo atual e variáveis globais
- adiciona no buffer que está vazio e incrementa a posicao, se tiver o buffer cheio manda sinal para thread de escrita em arquivo

**void t_logwr (void[] arg)**
- fica esperando pelo sinal da thread t_logrd
- abre o arquivo no modo anexo e escreve linha por linha do buffer cheio
- fecha o arquivo

**void t_print (void[] arg)**
- cria variáveis locais
- pega o mutex do terminal
- copia as variáveis a serem printadas para variáveis locais
- escreve no terminal os valores das variáveis
- larga o mutex do terminal

**void t_input (void[] arg)**
 - espera por um enter no terminal
- pega o mutex do terminal 
- pega os comando entrados no terminal e altera as referencias
- larga o mutex do terminal

**void t_hctrl (void[] arg)**
- aplica controle on-off com histerese

**void t_tctrl (void[] arg)**
- aplica controle on-off com histerese

**void t_alarm (void[] arg)**
- cria socket de comunicação
- espera pelo disparar do sinal go_on
- desliga a atuação do sistema de controle
- finaliza a thread (volta pro main no pthread_join)

SSS