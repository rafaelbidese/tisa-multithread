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
