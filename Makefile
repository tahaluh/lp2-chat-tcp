CC=gcc
CFLAGS=-Wall -Wextra -pthread -g -O2
LDFLAGS=-pthread

# Objetos comuns
COMMON_OBJS=tslog.o thread_safe_queue.o client_manager.o

# Binários principais
BINARIES=server client

all: $(BINARIES)

# Regras para objetos comuns
tslog.o: tslog.c tslog.h
	$(CC) $(CFLAGS) -c tslog.c -o tslog.o

thread_safe_queue.o: thread_safe_queue.c thread_safe_queue.h
	$(CC) $(CFLAGS) -c thread_safe_queue.c -o thread_safe_queue.o

client_manager.o: client_manager.c client_manager.h
	$(CC) $(CFLAGS) -c client_manager.c -o client_manager.o

# Servidor completo (thread-safe)
server: server.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) server.c $(COMMON_OBJS) -o server $(LDFLAGS)

# Cliente melhorado (com retry/timeout)
client: client.c
	$(CC) $(CFLAGS) client.c -o client $(LDFLAGS)

# Executar testes
test: $(BINARIES)
	@echo "=== Teste do sistema completo ==="
	@echo "Inicie o servidor em um terminal: ./server"
	@echo "Inicie clientes em outros terminais: ./client"
	@echo "Ou use o script de teste: ./test.sh"

# Limpeza
clean:
	rm -f $(BINARIES) $(COMMON_OBJS) *.o server.log

# Limpeza completa
distclean: clean
	rm -f *~ core

# Depuração com gdb
debug-server: server
	gdb ./server

debug-client: client
	gdb ./client

# Informações de build
info:
	@echo "=== INFORMAÇÕES DE BUILD ==="
	@echo "Compilador: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Linker flags: $(LDFLAGS)"
	@echo "Binários: $(BINARIES)"
	@echo "Objetos comuns: $(COMMON_OBJS)"
	@echo ""
	@echo "=== TARGETS DISPONÍVEIS ==="
	@echo "all       - Compila servidor e cliente"
	@echo "server    - Servidor thread-safe completo"
	@echo "client    - Cliente melhorado com retry"
	@echo "test      - Instruções para teste"
	@echo "clean     - Remove binários e objetos"
	@echo "distclean - Limpeza completa"
	@echo "debug-*   - Executa com gdb"
	@echo "info      - Esta informação"

.PHONY: all clean distclean test debug-server debug-client info
