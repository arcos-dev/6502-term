#!/bin/bash

# Script para executar todos os testes do emulador 6502
# Autor: Anderson Costa
# Versão: 1.0.0

set -e  # Parar em caso de erro

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Função para imprimir mensagens coloridas
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Função para verificar se um comando existe
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Função para limpar arquivos temporários
cleanup() {
    print_status "Limpando arquivos temporários..."
    make clean >/dev/null 2>&1 || true
}

# Função para executar testes unitários
run_unit_tests() {
    print_status "Executando testes unitários..."
    
    if make test; then
        print_success "Testes unitários passaram!"
        return 0
    else
        print_error "Testes unitários falharam!"
        return 1
    fi
}

# Função para executar teste funcional
run_functional_test() {
    print_status "Executando teste funcional..."
    
    if [ ! -f "6502_functional_test.bin" ]; then
        print_warning "Arquivo 6502_functional_test.bin não encontrado, pulando teste funcional"
        return 0
    fi
    
    if make functional_test; then
        print_success "Teste funcional passou!"
        return 0
    else
        print_error "Teste funcional falhou!"
        return 1
    fi
}

# Função para executar testes de memória (valgrind)
run_memory_tests() {
    if ! command_exists valgrind; then
        print_warning "Valgrind não encontrado, pulando testes de memória"
        return 0
    fi
    
    print_status "Executando testes de memória com Valgrind..."
    
    if make valgrind > valgrind.log 2>&1; then
        print_success "Testes de memória passaram!"
        return 0
    else
        print_error "Testes de memória falharam! Verifique valgrind.log"
        return 1
    fi
}

# Função para gerar relatório
generate_report() {
    local unit_result=$1
    local functional_result=$2
    local memory_result=$3
    
    echo ""
    echo "=========================================="
    echo "           RELATÓRIO DE TESTES"
    echo "=========================================="
    echo "Data/Hora: $(date)"
    echo "Sistema: $(uname -s) $(uname -m)"
    echo ""
    echo "Resultados:"
    echo "  Testes Unitários: $([ $unit_result -eq 0 ] && echo "PASSOU" || echo "FALHOU")"
    echo "  Teste Funcional:  $([ $functional_result -eq 0 ] && echo "PASSOU" || echo "FALHOU")"
    echo "  Testes de Memória: $([ $memory_result -eq 0 ] && echo "PASSOU" || echo "FALHOU")"
    echo ""
    
    if [ $unit_result -eq 0 ] && [ $functional_result -eq 0 ] && [ $memory_result -eq 0 ]; then
        print_success "🎉 TODOS OS TESTES PASSARAM!"
        echo "=========================================="
        return 0
    else
        print_error "❌ ALGUNS TESTES FALHARAM!"
        echo "=========================================="
        return 1
    fi
}

# Função principal
main() {
    print_status "Iniciando testes do emulador 6502..."
    
    # Verificar se estamos no diretório correto
    if [ ! -f "Makefile" ]; then
        print_error "Makefile não encontrado. Execute este script no diretório tests/"
        exit 1
    fi
    
    # Configurar trap para limpeza
    trap cleanup EXIT
    
    # Compilar todos os testes
    print_status "Compilando testes..."
    if ! make all; then
        print_error "Falha na compilação dos testes"
        exit 1
    fi
    
    # Executar testes
    local unit_result=1
    local functional_result=1
    local memory_result=1
    
    run_unit_tests
    unit_result=$?
    
    run_functional_test
    functional_result=$?
    
    run_memory_tests
    memory_result=$?
    
    # Gerar relatório
    generate_report $unit_result $functional_result $memory_result
    local overall_result=$?
    
    exit $overall_result
}

# Executar função principal
main "$@" 