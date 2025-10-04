
# Etapa 2 — Protótipo CLI Cliente/Servidor (v2-cli)

## 🔹 Objetivo
Esta versão (v2) implementa um protótipo mínimo de **cliente/servidor TCP** que integra a biblioteca de logging thread-safe (`tslog`). O servidor aceita múltiplos clientes simultâneos, retransmite mensagens (broadcast) e registra eventos em um arquivo de log.

---

## 📂 Estrutura relevante do projeto
```text
Makefile        # regras para compilar `server` e `client`
server.c        # servidor TCP concorrente (porta 8080)
client.c        # cliente TCP (modo interativo ou envio único via arg)
tslog.h/c       # biblioteca de logging thread-safe
test.sh         # script para simular múltiplos clientes
server.log      # arquivo de log gerado pelo servidor (apêndice)
saida.log       # arquivo de exemplo/registro antigo
```

---

## 🚀 Como compilar e executar (v2)

1) Compilar (gera os binários `server` e `client`):

```bash
make
```

2) Iniciar o servidor (escuta na porta 8080):

```bash
./server
```

3) Conectar um cliente (interativo):

```bash
./client
```

Enquanto rodar em modo interativo, o cliente exibirá um prompt `> `; tudo que for digitado será enviado ao servidor e retransmitido aos demais clientes conectados.

4) Enviar uma mensagem única (modo não interativo):

```bash
./client "Mensagem de exemplo"
```

Esse modo é usado pelo `test.sh` para disparar múltiplos clientes que enviam mensagens e se desconectam.

5) Rodar o script de testes (simula 5 clientes enviando mensagens):

```bash
./test.sh
```

6) Verificar o log do servidor:

```bash
cat server.log
```

Exemplo de entrada no `server.log` (formato gerado por `tslog`):

[2025-10-03 23:01:37] Novo cliente conectado.
[2025-10-03 23:01:37] Cliente 5: Olá do cliente 5
[2025-10-03 23:01:37] Cliente 6: Olá do cliente 6

---

## ✅ Observações e detalhes de implementação
- Porta do servidor: 8080 (definida em `server.c`).
- Máximo de clientes: o servidor usa um array com capacidade para `MAX_CLIENTS` (100).
- Logging: `tslog_init("server.log")` abre o arquivo `server.log` em modo append; `tslog_write()` protege escrita com `pthread_mutex`.
- Concurrency: o servidor cria uma thread por cliente com `pthread_create` e usa `pthread_detach`.
- Broadcast: mensagens recebidas de um cliente são retransmitidas para todos os outros clientes conectados.

---

## 📝 Changelog (v2)
- Migrado de uma biblioteca de logging isolada para um protótipo cliente/servidor TCP.
- Servidor concorrente que aceita múltiplos clientes e faz broadcast de mensagens.
- Integração com `tslog` para registrar eventos em `server.log`.
- Script `test.sh` para simular múltiplos clientes enviando mensagens automaticamente.

---

## 🔧 Notas de desenvolvimento
- Compilador/testado com `gcc` e POSIX threads (`-pthread`).
- Arquivos principais: `server.c`, `client.c`, `tslog.c`/`tslog.h`.
- Para limpeza de binários:

```bash
make clean
```


