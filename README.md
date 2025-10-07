
# Chat Multiusuário — Sistema Completo TCP Thread-Safe

## 🎯 Objetivo
Sistema completo de **chat multiusuário** implementando todos os requisitos da **Etapa 3**:
- Servidor TCP concorrente com threads por cliente
- Sincronização thread-safe (mutexes, condition variables, monitores)
- Autenticação, mensagens privadas, broadcast e filtros
- Tratamento robusto de erros e gerenciamento de recursos
- Logging concorrente integrado e documentação completa

---

## 📂 Estrutura do Projeto
```text
├── Makefile                    # Build system completo
├── README.md                   # Esta documentação
├── RELATORIO.md               # Relatório final com análise de IA
├── docs/
│   └── sequence_diagrams.md   # Diagramas de sequência obrigatórios
│
├── Código Principal:
│   ├── server.c               # Servidor completo thread-safe
│   ├── client.c               # Cliente melhorado com retry/timeout  
│   ├── thread_safe_queue.c/h  # Monitor com condition variables
│   ├── client_manager.c/h     # Gerenciador thread-safe de clientes
│   └── tslog.c/h              # Biblioteca logging thread-safe
│
└── test.sh                    # Script de teste automático
```

---

## 🚀 Como compilar e usar

### 1) Compilar tudo:
```bash
make all
```

### 2) Executar servidor:
```bash
./server
```
Saída esperada:
```
=== SERVIDOR DE CHAT MULTIUSUÁRIO v3 ===
✓ Componentes inicializados com sucesso
✓ Servidor rodando na porta 8080
✓ Thread de broadcast ativa
✓ Aguardando conexões...
✓ Pressione Ctrl+C para finalizar graciosamente
```

### 3) Conectar clientes:

**Modo interativo** (recomendado):
```bash
./client                       # Conecta a localhost
./client 192.168.1.100        # Conecta a IP específico
```

**Modo não-interativo** (mensagem única):
```bash
./client "Olá pessoal!"               # Para localhost  
./client 192.168.1.100 "Teste!"      # Para IP específico
```

### 4) Comandos disponíveis no chat:

```text
=== COMANDOS DO SERVIDOR ===
/auth <senha>          - Autenticar (senha: chat123)
/list                  - Listar usuários online  
/msg <user> <mensagem> - Mensagem privada
/nick <nome>           - Mudar nome de usuário
/help                  - Ver ajuda completa
/quit                  - Sair do chat

=== COMANDOS LOCAIS (CLIENTE) ===  
/help                  - Ver esta ajuda
/quit, /exit           - Sair (local)
Ctrl+C                 - Forçar saída
```

### 5) Fluxo típico de uso:
```bash
# Terminal 1 - Servidor
./server

# Terminal 2 - Cliente A  
./client
> /auth chat123
✓ Autenticado com sucesso! Bem-vindo ao chat.
> /nick Alice
✓ Nome alterado de User_45678 para Alice
> Olá pessoal!

# Terminal 3 - Cliente B
./client  
> /auth chat123
✓ Autenticado com sucesso! Bem-vindo ao chat.
[Alice]: Olá pessoal!
> /msg Alice Oi, tudo bem?
✓ Mensagem privada enviada para Alice
```

### 6) Verificar logs:
```bash
cat server.log
```
Exemplo de saída:
```
[2025-10-06 15:30:12] === SERVIDOR DE CHAT INICIANDO ===
[2025-10-06 15:30:15] Cliente adicionado: User_45678 (127.0.0.1:45678) socket=4
[2025-10-06 15:30:18] Cliente autenticado: Alice
[2025-10-06 15:30:18] Cliente Alice entrou no chat
[2025-10-06 15:30:25] Broadcast de Alice para 2 clientes: [Alice]: Olá pessoal!
[2025-10-06 15:30:30] Mensagem privada: Bob -> Alice
```

---

## 🔧 Build System e Targets

### Targets principais:
```bash
make all          # Compila servidor e cliente
make server       # Servidor thread-safe completo  
make client       # Cliente melhorado com retry
make clean        # Remove binários e objetos
make info         # Mostra informações de build
```

### Targets de desenvolvimento:
```bash
make debug-server # Executa server no gdb
make debug-client # Executa client no gdb  
make test         # Instruções para testes
```

### Binários disponíveis:
```bash
./server     # Servidor completo thread-safe
./client     # Cliente melhorado com tratamento de erros
```

---

## ✅ Funcionalidades Implementadas

### Obrigatórias (Tema A) ✅
- ✅ **Servidor TCP concorrente** - aceita múltiplos clientes simultâneos
- ✅ **Thread por cliente** - cada conexão tem thread dedicada  
- ✅ **Broadcast de mensagens** - retransmite para todos os clientes
- ✅ **Logging concorrente** - usando `libtslog` thread-safe
- ✅ **Cliente CLI** - conectar, enviar/receber mensagens
- ✅ **Proteção de estruturas** - lista de clientes e filas protegidas

### Opcionais ✅
- ✅ **Autenticação simples** - senha obrigatória (`chat123`)
- ✅ **Mensagens privadas** - comando `/msg <user> <mensagem>`
- ✅ **Filtros de palavras** - bloqueia conteúdo proibido

### Requisitos Gerais ✅
- ✅ **Threads** - `pthreads` para concorrência
- ✅ **Exclusão mútua** - `pthread_mutex_t` em todas estruturas
- ✅ **Semáforos/condvars** - `pthread_cond_t` para sincronização
- ✅ **Monitores** - `ThreadSafeQueue` e `ClientManager`
- ✅ **Sockets** - TCP com tratamento robusto de erros
- ✅ **Gerenciamento recursos** - cleanup gracioso com signals
- ✅ **Tratamento erros** - mensagens amigáveis e recovery
- ✅ **Logging obrigatório** - `libtslog` integrado
- ✅ **Documentação** - diagramas de sequência completos
- ✅ **Build** - `Makefile` funcional em Linux

---

## 🏗️ Arquitetura Thread-Safe

### Componentes Principais:

1. **ThreadSafeQueue** (Monitor):
   - Fila circular thread-safe com condition variables
   - Produtores bloqueiam se cheia, consumidores se vazia
   - Usado para comunicação cliente→broadcast

2. **ClientManager** (Monitor):  
   - Gerencia lista de clientes com proteção mutex
   - Coordena slots disponíveis via condition variables
   - Operações atômicas de add/remove/broadcast

3. **TSLog** (Thread-Safe):
   - Serializa escritas no arquivo de log
   - Mutex protege acesso concorrente ao arquivo

### Fluxo de Mensagens:
```
Cliente → handle_client() → ThreadSafeQueue → broadcast_worker() → ClientManager → Outros Clientes
```

### Sincronização:
- **Exclusão mútua**: 3 mutexes (queue, clients, log)
- **Condition variables**: 4 condvars (not_empty, not_full, slot_available, client_connected)
- **Padrões**: Producer-Consumer, Monitor, Reader-Writer

---

## 📊 Testes e Verificação

### Teste básico (script automático):
```bash
./test.sh  # Envia 5 mensagens simultâneas
```

### Teste de stress:
```bash
# Terminal 1 - Servidor
./server

# Terminais 2-6 - Múltiplos clientes
for i in {1..5}; do
  (./client <<< $'/auth chat123\nUsuário '$i' conectado\n/quit') &
done
wait
```

### Teste de funcionalidades:
1. ✅ **Concorrência**: 10+ clientes simultâneos  
2. ✅ **Autenticação**: Bloqueia mensagens sem auth
3. ✅ **Broadcast**: Mensagem chega a todos os clientes
4. ✅ **Mensagens privadas**: `/msg` funciona corretamente
5. ✅ **Filtro**: Palavras proibidas são bloqueadas  
6. ✅ **Logs**: Todas operações registradas em `server.log`
7. ✅ **Cleanup**: Ctrl+C finaliza graciosamente
8. ✅ **Recovery**: Cliente reconecta após falha de rede

---

## 📋 Status dos Requisitos

| Requisito | Status | Implementação |
|-----------|--------|---------------|
| Servidor TCP concorrente | ✅ | `server.c:590-605` |
| Thread por cliente | ✅ | `pthread_create()` para cada conexão |
| Broadcast | ✅ | `ThreadSafeQueue` + `broadcast_worker()` |
| Logging concorrente | ✅ | `tslog.c` com mutex |
| Cliente CLI | ✅ | `client.c` completo |
| Proteção estruturas | ✅ | Todos os componentes thread-safe |
| Exclusão mútua | ✅ | 3 mutexes principais |
| Condvars/semáforos | ✅ | 4 condition variables |
| Monitores | ✅ | `ThreadSafeQueue`, `ClientManager` |
| Gerenc. recursos | ✅ | Signal handlers + cleanup |
| Tratamento erros | ✅ | Recovery + mensagens amigáveis |
| Documentação | ✅ | `docs/sequence_diagrams.md` |
| Build funcional | ✅ | `Makefile` completo |
| **Análise IA** | ✅ | `RELATORIO.md` - baixo risco |

**Status Geral**: ✅ **ETAPA 3 COMPLETA - 100% DOS REQUISITOS ATENDIDOS**

---

## � Documentação Técnica

- **Relatório Final**: `RELATORIO.md` - Mapeamento requisitos→código + análise IA
- **Diagramas**: `docs/sequence_diagrams.md` - 5 diagramas de sequência  
- **Build**: `make info` - Informações detalhadas de compilação

---

## 💡 Uso Recomendado

**Para demonstração**:
```bash
make all && ./server     # Em um terminal
./client                 # Em outro terminal
```

**Para desenvolvimento**:
```bash
make debug-server        # Debug do servidor
make debug-client        # Debug do cliente  
```

**Para testes**:
```bash
make test               # Ver instruções
./test.sh               # Teste automatizado
```

---

## 🎓 Conclusão

Este projeto implementa **completamente** todos os requisitos da **Etapa 3** do tema **"Servidor de Chat Multiusuário (TCP)"**, incluindo:

- ✅ Funcionalidades obrigatórias e opcionais do tema
- ✅ Todos os requisitos gerais de concorrência  
- ✅ Logging integrado thread-safe
- ✅ Documentação completa (diagramas + relatório)
- ✅ Análise crítica com IA (race conditions, deadlocks)
- ✅ Build system robusto e testes funcionais

**Sistema pronto para avaliação final.**


