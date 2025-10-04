# Etapa 1 — Logging Thread-Safe (v1-logging)

## 🔹 Objetivo
Implementação da biblioteca **libtslog** para registro de logs com segurança em ambiente concorrente.  
A biblioteca garante exclusão mútua com `pthread_mutex` e grava mensagens em arquivo com **timestamp**.

---

## 📂 Estrutura do Projeto
├── tslog.h      # Cabeçalho da biblioteca  
├── tslog.c      # Implementação da biblioteca  
├── main.c       # Programa de teste (simulação com múltiplas threads)  
└── Makefile     # Automação da compilação  

---

## 🚀 Como executar

### 1. Clonar o repositório e acessar a tag da Etapa 1
$ git clone https://github.com/tahaluh/lp2-chat-tcp.git

$ cd lp2-trabalho-final  

$ git checkout v1-logging  

### 2. Compilar
$ make  

### 3. Rodar o programa de teste
$ ./main  

### 4. Verificar o arquivo de log
$ cat saida.log  

Exemplo de saída:

[2025-10-03 23:40:12] Thread 1 - mensagem 1  
[2025-10-03 23:40:12] Thread 2 - mensagem 1  
[2025-10-03 23:40:12] Thread 3 - mensagem 1  
[2025-10-03 23:40:12] Thread 4 - mensagem 1  
...  

---

## ✅ Observações
- Testado em **Linux (Ubuntu 22.04)** com `gcc` e `pthread`.  
- O log é salvo no arquivo `saida.log` no mesmo diretório do programa.  
- Cada linha é escrita de forma concorrente, sem corromper o arquivo.  
- Uso de `pthread_mutex` garante exclusão mútua na escrita do log.  


