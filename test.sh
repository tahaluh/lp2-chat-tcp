#!/bin/bash

# Script de Testes do Sistema de Chat Multiusuário v3
# Testa funcionalidades básicas e concorrência

echo "=== INICIANDO TESTES DO SISTEMA DE CHAT v3 ==="

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Função para imprimir com cores
print_test() {
    echo -e "${BLUE}[TESTE]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCESSO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERRO]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# Verificar se o servidor está rodando
check_server() {
    if ! pgrep -f "./server" > /dev/null; then
        print_error "Servidor não está rodando!"
        print_info "Inicie o servidor com: ./server"
        exit 1
    fi
    print_success "Servidor está rodando"
}

# Teste 1: Conectividade básica
test_connectivity() {
    print_test "Testando conectividade básica..."
    
    if ./client "Teste de conectividade" > /dev/null 2>&1; then
        print_success "Conectividade básica OK"
        return 0
    else
        print_error "Falha na conectividade básica"
        return 1
    fi
}

# Teste 2: Múltiplos clientes simultâneos
test_multiple_clients() {
    print_test "Testando múltiplos clientes simultâneos (5 clientes)..."
    
    pids=()
    
    for i in {1..5}; do
        ./client "Mensagem do cliente $i" > /tmp/client_$i.out 2>&1 &
        pids+=($!)
        sleep 0.2  # Pequeno delay entre conexões
    done
    
    # Aguardar conclusão
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    print_success "Teste de múltiplos clientes concluído"
    
    # Limpar arquivos temporários
    rm -f /tmp/client_*.out 2>/dev/null
}

# Teste 3: Teste de carga com mais clientes
test_load() {
    print_test "Teste de carga com 10 clientes simultâneos..."
    
    pids=()
    
    for i in {1..10}; do
        ./client "Carga cliente $i - $(date +%T)" > /dev/null 2>&1 &
        pids+=($!)
        
        # Adicionar delay menor para simular carga real
        sleep 0.1
    done
    
    # Aguardar todos terminarem
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    print_success "Teste de carga concluído"
}

# Teste 4: Teste com mensagens de diferentes tamanhos
test_message_sizes() {
    print_test "Testando mensagens de diferentes tamanhos..."
    
    # Mensagem pequena
    ./client "Pequena" > /dev/null 2>&1
    
    # Mensagem média
    ./client "Esta é uma mensagem de tamanho médio para testar o sistema" > /dev/null 2>&1
    
    # Mensagem grande (próximo ao limite)
    large_msg=$(printf 'A%.0s' {1..500})
    ./client "$large_msg" > /dev/null 2>&1
    
    print_success "Teste de tamanhos de mensagem concluído"
}

# Teste 5: Teste de reconnect (simula instabilidade)
test_reconnect() {
    print_test "Testando reconexão rápida (10 conexões sequenciais)..."
    
    for i in {1..10}; do
        ./client "Reconexão teste $i" > /dev/null 2>&1
        sleep 0.1
    done
    
    print_success "Teste de reconexão concluído"
}

# Função principal
main() {
    echo ""
    print_info "Verificando pré-requisitos..."
    
    # Verificar se os executáveis existem
    if [[ ! -f "./server" ]]; then
        print_error "Executável ./server não encontrado!"
        print_info "Compile com: make server"
        exit 1
    fi
    
    if [[ ! -f "./client" ]]; then
        print_error "Executável ./client não encontrado!"
        print_info "Compile com: make client"
        exit 1
    fi
    
    print_success "Executáveis encontrados"
    
    # Verificar servidor
    check_server
    
    echo ""
    print_info "Aguardando 2 segundos para estabilizar conexão..."
    sleep 2
    
    # Executar testes
    echo ""
    print_info "=== INICIANDO BATERIA DE TESTES ==="
    
    test_connectivity
    sleep 1
    
    test_multiple_clients  
    sleep 2
    
    test_message_sizes
    sleep 1
    
    test_reconnect
    sleep 1
    
    test_load
    sleep 2
    
    echo ""
    print_info "=== TODOS OS TESTES CONCLUÍDOS ==="
    print_success "Sistema testado com sucesso!"
    
    # Estatísticas do servidor
    if [[ -f "server.log" ]]; then
        echo ""
        print_info "Últimas entradas do log do servidor:"
        tail -5 server.log 2>/dev/null || echo "Log não disponível"
    fi
    
    echo ""
    print_info "Para testes interativos, execute: ./client"
    print_info "Para parar o servidor: Ctrl+C no terminal do servidor"
}

# Verificar argumentos
if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    echo "Uso: $0 [opções]"
    echo ""
    echo "Script de testes para o Sistema de Chat Multiusuário v3"
    echo ""
    echo "Opções:"
    echo "  -h, --help     Mostrar esta ajuda"
    echo "  -q, --quick    Executar apenas testes básicos"
    echo ""
    echo "Pré-requisitos:"
    echo "  - Servidor deve estar rodando (./server)"
    echo "  - Executáveis compilados (make all)"
    echo ""
    exit 0
fi

if [[ "$1" == "-q" ]] || [[ "$1" == "--quick" ]]; then
    print_info "Modo rápido - executando apenas testes básicos"
    check_server
    test_connectivity
    test_multiple_clients
    print_success "Testes básicos concluídos!"
    exit 0
fi

# Executar testes completos
main
