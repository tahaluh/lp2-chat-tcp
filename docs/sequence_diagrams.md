# Diagramas de Sequência - Chat Multiusuário v3

## 1. Conexão de Cliente e Autenticação

```mermaid
sequenceDiagram
    participant C as Cliente
    participant S as Servidor
    participant HT as HandleClientThread
    participant CM as ClientManager
    participant Q as ThreadSafeQueue
    participant L as TSLog

    C->>S: TCP connect() - porta 8080
    S->>S: accept() - novo socket
    S->>CM: client_manager_add(socket, temp_username, ip, port)
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: Adiciona cliente ao array[MAX_CLIENTS]
    CM->>CM: client.authenticated = false
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM->>L: tslog_write("Cliente adicionado: User_PORT")
    
    S->>S: pthread_create(&tid, handle_client, &socket)
    S->>S: pthread_detach(tid)
    
    HT->>C: Mensagem de boas-vindas + instruções
    Note over HT,C: "Digite /auth chat123 para começar!"
    
    C->>HT: "/auth chat123"
    HT->>HT: process_command(client_sock, "/auth chat123")
    HT->>CM: client_manager_authenticate(socket, "chat123")
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: if (strcmp(password, "chat123") == 0)
    CM->>CM: client.authenticated = true
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM->>L: tslog_write("Cliente autenticado: username")
    CM-->>HT: return 0 (sucesso)
    
    HT->>C: "✓ Autenticado com sucesso! Bem-vindo ao chat."
    HT->>Q: tsqueue_enqueue(MSG_JOIN)
    Q->>Q: pthread_mutex_lock(&queue_mutex)
    Q->>Q: while(count >= MAX_QUEUE_SIZE) pthread_cond_wait(&not_full)
    Q->>Q: queue[rear] = join_message
    Q->>Q: count++, rear = (rear + 1) % MAX_QUEUE_SIZE
    Q->>Q: pthread_cond_signal(&not_empty)
    Q->>Q: pthread_mutex_unlock(&queue_mutex)
```

## 2. Broadcast de Mensagem

```mermaid
sequenceDiagram
    participant C1 as Cliente1
    participant HT as HandleClientThread
    participant Q as ThreadSafeQueue
    participant BW as BroadcastWorkerThread
    participant CM as ClientManager
    participant C2 as Cliente2
    participant C3 as Cliente3
    participant L as TSLog

    C1->>HT: "Olá pessoal!"
    HT->>HT: recv(client_sock, buffer, BUFFER_SIZE)
    
    alt Cliente não autenticado
        HT->>C1: "⚠ Você precisa se autenticar: /auth <senha>"
        HT->>L: tslog_write("Mensagem rejeitada (não autenticado)")
    else Cliente autenticado
        HT->>HT: contains_profanity(buffer) - verifica lista
        
        alt Contém palavrão
            HT->>C1: "⚠ AVISO: Mensagem contém conteúdo proibido"
            HT->>L: tslog_write("Mensagem bloqueada por filtro")
        else Mensagem limpa
            HT->>HT: snprintf("[%s]: %s", username, buffer)
            HT->>Q: tsqueue_enqueue(MSG_BROADCAST, formatted_msg)
            Q->>Q: pthread_mutex_lock(&queue_mutex)
            Q->>Q: while(count >= MAX_QUEUE_SIZE) pthread_cond_wait(&not_full)
            Q->>Q: queue[rear] = broadcast_message
            Q->>Q: count++, rear++
            Q->>Q: pthread_cond_signal(&not_empty)
            Q->>Q: pthread_mutex_unlock(&queue_mutex)
            
            HT->>HT: printf("[Chat] %s", formatted_msg)
        end
    end
    
    BW->>Q: tsqueue_dequeue(&message)
    Q->>Q: pthread_mutex_lock(&queue_mutex)
    Q->>Q: while(count == 0) pthread_cond_wait(&not_empty)
    Q->>Q: message = queue[front]
    Q->>Q: count--, front++
    Q->>Q: pthread_cond_signal(&not_full)
    Q->>Q: pthread_mutex_unlock(&queue_mutex)
    
    BW->>BW: switch(message.type) - MSG_BROADCAST
    BW->>CM: client_manager_broadcast(message.content, sender_fd)
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: for(i=0; i<MAX_CLIENTS; i++)
    CM->>C2: send("[Cliente1]: Olá pessoal!", MSG_NOSIGNAL)
    CM->>C3: send("[Cliente1]: Olá pessoal!", MSG_NOSIGNAL)
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM-->>BW: return sent_count
    
    BW->>L: tslog_write("Broadcast de Cliente1 para 2 clientes: [Cliente1]: Olá pessoal!")
```

## 3. Mensagem Privada

```mermaid
sequenceDiagram
    participant C1 as Cliente1
    participant HT as HandleClientThread
    participant CM as ClientManager
    participant Q as ThreadSafeQueue
    participant BW as BroadcastWorkerThread
    participant C2 as Cliente2
    participant L as TSLog

    C1->>HT: "/msg User_8080 Olá privado!"
    HT->>HT: process_command(client_sock, "/msg User_8080 Olá privado!")
    HT->>HT: strncmp(command, "/msg ", 5) == 0
    HT->>HT: Parse: target="User_8080", message="Olá privado!"
    
    HT->>CM: client_manager_find_by_username("User_8080")
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: for(i=0; i<MAX_CLIENTS; i++) strcmp(username)
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM-->>HT: return ClientInfo* ou NULL
    
    alt Cliente encontrado e online
        HT->>Q: tsqueue_enqueue(MSG_PRIVATE)
        Q->>Q: pthread_mutex_lock(&queue_mutex)
        Q->>Q: message.type = MSG_PRIVATE
        Q->>Q: strcpy(message.username, "Cliente1")
        Q->>Q: strcpy(message.target, "User_8080")
        Q->>Q: strcpy(message.content, "Olá privado!")
        Q->>Q: queue[rear] = message, count++
        Q->>Q: pthread_cond_signal(&not_empty)
        Q->>Q: pthread_mutex_unlock(&queue_mutex)
        
        BW->>Q: tsqueue_dequeue(&message)
        Q->>Q: Protocolo thread-safe de dequeue
        
        BW->>BW: switch(message.type) - MSG_PRIVATE
        BW->>CM: client_manager_send_private(username, target, content)
        CM->>CM: pthread_mutex_lock(&mutex)
        CM->>CM: Encontra socket do target_user
        CM->>C2: send("[PRIVADA de Cliente1]: Olá privado!", MSG_NOSIGNAL)
        CM->>CM: pthread_mutex_unlock(&mutex)
        
        BW->>L: tslog_write("Mensagem privada: Cliente1 -> User_8080")
        HT->>C1: "✓ Mensagem privada enviada para User_8080"
        
    else Cliente não encontrado ou offline
        HT->>C1: "✗ Usuário 'User_8080' não encontrado ou offline"
        HT->>L: tslog_write("Tentativa de mensagem privada para usuário inexistente")
    end
```

## 4. Desconexão de Cliente

```mermaid
sequenceDiagram
    participant C as Cliente
    participant HT as HandleClientThread
    participant CM as ClientManager
    participant Q as ThreadSafeQueue
    participant BW as BroadcastWorkerThread
    participant Others as OutrosClientes
    participant L as TSLog

    Note over C: Cliente fecha conexão, Ctrl+C ou digita /quit
    
    alt Comando /quit
        C->>HT: "/quit"
        HT->>HT: process_command(client_sock, "/quit")
        HT->>C: "Até logo! Desconectando..."
        HT->>HT: return -1 (sinaliza desconexão)
    else Fechamento abrupto
        C->>C: close(socket) ou Ctrl+C
        HT->>HT: recv() retorna 0 ou errno
        Note over HT: Loop while(server_running && recv > 0) termina
    end
    
    HT->>CM: client_manager_find_by_socket(client_sock)
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: Busca cliente no array
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM-->>HT: return ClientInfo*
    
    alt Cliente estava autenticado
        HT->>Q: tsqueue_enqueue(MSG_LEAVE)
        Q->>Q: pthread_mutex_lock(&queue_mutex)
        Q->>Q: message.type = MSG_LEAVE
        Q->>Q: strcpy(message.username, client.username)
        Q->>Q: queue[rear] = leave_message, count++
        Q->>Q: pthread_cond_signal(&not_empty)
        Q->>Q: pthread_mutex_unlock(&queue_mutex)
        
        BW->>Q: tsqueue_dequeue(&message)
        Q->>Q: Protocolo thread-safe de dequeue
        
        BW->>BW: switch(message.type) - MSG_LEAVE
        BW->>CM: client_manager_broadcast(leave_message, -1)
        CM->>CM: pthread_mutex_lock(&mutex)
        CM->>Others: send("*** User_8080 saiu do chat ***", MSG_NOSIGNAL)
        CM->>CM: pthread_mutex_unlock(&mutex)
        
        BW->>L: tslog_write("Cliente User_8080 saiu do chat")
    end
    
    HT->>HT: close(client_sock)
    HT->>CM: client_manager_remove(client_sock)
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>CM: clients[i].socket = -1, count--
    CM->>CM: pthread_cond_signal(&slot_available)  
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM->>L: tslog_write("Cliente removido: User_8080 (socket=N)")
    
    HT->>HT: return NULL (thread termina - pthread_detach)
```

## 5. Finalização Gracioso do Servidor

```mermaid
sequenceDiagram
    participant Admin as Administrador
    participant S as ServidorMain
    participant BW as BroadcastWorkerThread
    participant HT as HandleClientThreads
    participant CM as ClientManager
    participant Q as ThreadSafeQueue
    participant Clients as TodosClientes
    participant L as TSLog

    Admin->>S: SIGINT (Ctrl+C) ou SIGTERM
    S->>S: signal_handler(sig)
    S->>S: printf("Recebido sinal %d, finalizando...")
    S->>S: server_running = 0
    
    S->>S: shutdown(server_socket, SHUT_RDWR)
    S->>S: close(server_socket), server_socket = -1
    
    S->>Q: tsqueue_enqueue(SHUTDOWN message)
    Q->>Q: pthread_mutex_lock(&queue_mutex)
    Q->>Q: shutdown_msg.content = "SHUTDOWN"
    Q->>Q: queue[rear] = shutdown_msg, count++
    Q->>Q: pthread_cond_signal(&not_empty)
    Q->>Q: pthread_mutex_unlock(&queue_mutex)
    
    Note over S: Loop accept() termina (server_running = 0)
    S->>S: printf("Iniciando processo de finalização...")
    S->>L: tslog_write("Iniciando finalização gracioso")
    
    BW->>Q: tsqueue_dequeue(&message)
    Q->>Q: Protocolo thread-safe de dequeue
    BW->>BW: if (strcmp(message.content, "SHUTDOWN") == 0)
    BW->>L: tslog_write("Thread de broadcast recebeu sinal de shutdown")
    BW->>BW: break (sai do loop while)
    
    S->>Q: Envia segunda mensagem SHUTDOWN (garantia)
    S->>S: printf("Aguardando thread de broadcast finalizar...")
    S->>BW: pthread_join(broadcast_thread, NULL)
    BW->>BW: return NULL (thread finalizada)
    S->>S: printf("Thread de broadcast finalizada.")
    
    Note over HT: Threads de cliente terminam naturalmente
    Note over HT: (recv() falha quando sockets fecham)
    
    S->>S: if (server_socket >= 0) close(server_socket)
    S->>S: printf("Finalizando componentes...")
    
    S->>Q: tsqueue_destroy()
    Q->>Q: pthread_mutex_lock(&queue_mutex)
    Q->>Q: pthread_cond_broadcast(&not_empty)
    Q->>Q: pthread_cond_broadcast(&not_full)
    Q->>Q: pthread_mutex_unlock(&queue_mutex)
    Q->>Q: pthread_mutex_destroy(&queue_mutex)
    Q->>Q: pthread_cond_destroy(&not_empty)
    Q->>Q: pthread_cond_destroy(&not_full)
    
    S->>CM: client_manager_destroy()
    CM->>CM: pthread_mutex_lock(&mutex)
    CM->>Clients: Fecha todos os sockets restantes
    CM->>CM: pthread_cond_broadcast(&slot_available)
    CM->>CM: pthread_mutex_unlock(&mutex)
    CM->>CM: pthread_mutex_destroy(&mutex)
    CM->>CM: pthread_cond_destroy(&slot_available)
    
    S->>L: tslog_write("=== SERVIDOR DE CHAT FINALIZADO ===")
    S->>L: tslog_close()
    S->>S: printf("✓ Servidor finalizado com sucesso.")
    S->>S: return 0 (exit limpo)
```

## 6. Finalização do Cliente

```mermaid
sequenceDiagram
    participant User as Usuário
    participant C as ClienteMain
    participant RT as ReceiveThread
    participant S as Servidor

    alt Modo Interativo - Ctrl+C
        User->>C: SIGINT (Ctrl+C)
        C->>C: signal_handler(SIGINT)
        C->>C: printf("Recebido sinal 2, desconectando...")
        C->>C: client_running = 0
        C->>C: shutdown(sock, SHUT_RDWR)
        C->>C: close(sock), sock = -1
        C->>RT: pthread_cancel(recv_thread)
        C->>C: printf("✓ Desconectado.")
        C->>C: exit(0) - Finalização forçada
        
    else Modo Interativo - /quit
        User->>C: "/quit"
        C->>C: strcmp(input, "/quit") == 0
        C->>C: printf("Saindo do chat...")
        C->>S: send_message("/quit")
        S->>C: "Até logo! Desconectando..."
        C->>C: break (sai do loop while)
        C->>C: client_running = 0
        C->>C: close(sock), sock = -1
        C->>RT: pthread_cancel(recv_thread)
        C->>RT: pthread_detach(recv_thread)
        C->>C: printf("✓ Desconectado do servidor.")
        C->>C: return EXIT_SUCCESS
        
    else Modo Não-Interativo
        C->>S: send_message("/auth chat123")
        C->>C: sleep(1)
        C->>S: send_message("Mensagem única")
        C->>C: printf("✓ Mensagem enviada")
        C->>C: sleep(1)
        C->>C: close(sock)
        C->>C: return EXIT_SUCCESS
        
    else Desconexão do Servidor
        S->>S: close(client_socket)
        RT->>RT: recv() retorna 0
        RT->>RT: printf("Servidor desconectou.")
        RT->>RT: client_running = 0
        RT->>RT: return NULL (thread termina)
        C->>C: fgets() pode falhar ou continuar
        C->>C: Loop detecta client_running = 0
        C->>C: break, finalização normal
    end
```

## Sincronização e Proteção

### Componentes Thread-Safe Implementados:

1. **ThreadSafeQueue (Monitor Pattern)**:
   - **Mutex**: `pthread_mutex_t queue_mutex`
   - **Condition Variables**: 
     - `pthread_cond_t not_empty` - Sinaliza quando há mensagens
     - `pthread_cond_t not_full` - Sinaliza quando há espaço
   - **Operações Atômicas**: 
     - `tsqueue_enqueue()` - Producer bloqueia se fila cheia
     - `tsqueue_dequeue()` - Consumer bloqueia se fila vazia
   - **Capacidade**: `MAX_QUEUE_SIZE` (100 mensagens)

2. **ClientManager (Reader-Writer Pattern)**:
   - **Mutex**: `pthread_mutex_t mutex`
   - **Condition Variable**: `pthread_cond_t slot_available`
   - **Operações Protegidas**:
     - `client_manager_add()` - Adiciona cliente thread-safe
     - `client_manager_remove()` - Remove cliente thread-safe
     - `client_manager_broadcast()` - Envia para todos os clientes
     - `client_manager_authenticate()` - Atualiza status de autenticação
     - `client_manager_find_by_*()` - Busca thread-safe
   - **Capacidade**: `MAX_CLIENTS` (10 clientes simultâneos)

3. **TSLog (Synchronized Logging)**:
   - **Mutex**: `pthread_mutex_t mutex_log`
   - **Operações**:
     - `tslog_write()` - Escrita thread-safe com timestamp
     - `tslog_init()` / `tslog_close()` - Gerenciamento seguro do arquivo
   - **Formato**: `[YYYY-MM-DD HH:MM:SS] mensagem`

### Padrões de Concorrência Aplicados:

1. **Producer-Consumer**: 
   - **Produtores**: HandleClientThreads (N threads)
   - **Consumidor**: BroadcastWorkerThread (1 thread)
   - **Buffer**: ThreadSafeQueue com sincronização via condition variables

2. **Thread Pool Dinâmico**:
   - **Threads criadas sob demanda**: `pthread_create()` para cada cliente
   - **Cleanup automático**: `pthread_detach()` para threads de cliente
   - **Thread persistente**: BroadcastWorker roda durante toda vida do servidor

3. **Monitor Pattern**:
   - **ThreadSafeQueue**: Encapsula toda lógica de sincronização internamente
   - **ClientManager**: Protege estruturas de dados compartilhadas
   - **Invariantes preservadas**: Nunca há condições de corrida

4. **Graceful Shutdown**:
   - **Servidor**: Sinal especial "SHUTDOWN" na fila para finalizar BroadcastWorker
   - **Cliente**: `exit(0)` forçado no signal handler para evitar deadlock
   - **Cleanup**: Todos os mutexes e condition variables são devidamente destruídos

### Tratamento de Condições de Corrida:

- ✅ **Lista de clientes**: Protegida por mutex em todas as operações
- ✅ **Fila de mensagens**: Implementa protocolo wait/signal correto
- ✅ **Logs**: Escritas serializadas com timestamp thread-safe  
- ✅ **Autenticação**: Status protegido contra modificação concorrente
- ✅ **Sockets**: Fechamento thread-safe com `shutdown()` antes de `close()`
- ✅ **Contadores**: Atualizados atomicamente dentro de seções críticas