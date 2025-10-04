# Etapa 2 — Protótipo CLI Cliente/Servidor (v2-cli)

## 🔹 Objetivo
Implementar um protótipo **cliente/servidor TCP** que utiliza a biblioteca `libtslog` (Etapa 1) para registrar conexões e mensagens de forma thread-safe.  
O sistema simula um **chat mínimo em linha de comando** com múltiplos clientes.

---

## 📂 Estrutura do Projeto
```text
├── tslog.h          # Cabeçalho da biblioteca de logging
├── tslog.c          # Implementação da biblioteca
├── server.c         # Servidor TCP concorrente
├── client.c         # Cliente TCP (modo interativo ou automático)
├── Makefile         # Automação da compilação
└── test_clients.sh  # Script para simular múltiplos clientes

---

## 🚀 Como executar

### 1. Clonar o repositório e acessar a tag da Etapa 1
$ git clone https://github.com/tahaluh/lp2-chat-tcp.git

$ cd lp2-chat-tcp  

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


