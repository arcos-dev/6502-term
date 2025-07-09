# Testes do Emulador 6502

Este diretório contém um sistema abrangente de testes para validar as funcionalidades do emulador 6502.

## Estrutura dos Testes

### 1. Testes Unitários (`unit_tests.c`)
Testes individuais para cada componente do emulador:

- **Inicialização da CPU**: Verifica se os registros são inicializados corretamente
- **Reset da CPU**: Testa o comportamento do reset
- **Leitura/Escrita de Memória**: Valida operações de memória
- **Instruções de Carregamento**: LDA, LDX, LDY
- **Instruções de Armazenamento**: STA, STX, STY
- **Instruções de Transferência**: TAX, TAY, TXA, TYA
- **Operações de Stack**: PHA, PHP, PLA, PLP
- **Instruções Aritméticas**: ADC, SBC, INC, DEC
- **Instruções Lógicas**: AND, ORA, EOR
- **Instruções de Deslocamento**: ASL, LSR, ROL, ROR
- **Instruções de Branch**: BNE, BEQ, BCC, BCS, etc.
- **Instruções de Jump**: JMP, JSR, RTS
- **Flags de Status**: Verifica o comportamento das flags
- **Modos de Endereçamento**: Testa diferentes modos de endereçamento
- **Interrupções**: IRQ, NMI
- **Breakpoints**: Sistema de breakpoints

### 2. Teste Funcional (`functional_test.c`)
Executa o teste funcional completo do 6502 (`6502_functional_test.bin`):

- Carrega o binário de teste
- Executa até 1 milhão de ciclos
- Detecta loops infinitos
- Verifica o resultado final
- Mostra o estado da CPU

### 3. Script de Automação (`run_tests.sh`)
Script bash que executa todos os testes automaticamente:

- Compila todos os testes
- Executa testes unitários
- Executa teste funcional
- Executa testes de memória (valgrind)
- Gera relatório completo

## Como Usar

### Compilação
```bash
# Compilar todos os testes
make all

# Compilar apenas testes unitários
make unit_tests

# Compilar apenas teste funcional
make functional_test
```

### Execução
```bash
# Executar todos os testes (recomendado)
./run_tests.sh

# Executar testes unitários
make test

# Executar teste funcional
make functional_test

# Executar com valgrind (detecção de vazamentos)
make valgrind

# Executar com gdb (debug)
make debug
```

### Limpeza
```bash
# Limpar arquivos compilados
make clean
```

## Comandos Disponíveis

| Comando | Descrição |
|---------|-----------|
| `make` | Compilar todos os testes |
| `make test` | Compilar e executar testes unitários |
| `make functional_test` | Compilar e executar teste funcional |
| `make clean` | Limpar arquivos compilados |
| `make valgrind` | Executar com valgrind (detecção de vazamentos) |
| `make debug` | Executar com gdb para debug |
| `./run_tests.sh` | Executar todos os testes automaticamente |

## Interpretação dos Resultados

### Testes Unitários
- **✓** = Teste passou
- **✗** = Teste falhou

O sistema mostra:
- Total de testes executados
- Número de testes que passaram
- Número de testes que falharam
- Taxa de sucesso em porcentagem

### Teste Funcional
- **🎉 Teste funcional PASSOU!** = Sucesso
- **❌ Teste funcional FALHOU!** = Falha

### Valgrind
- Verifica vazamentos de memória
- Detecta acesso a memória inválida
- Gera relatório detalhado em `valgrind.log`

## Arquivos de Teste

### `6502_functional_test.bin`
Este é o teste funcional oficial do 6502 que valida:
- Todas as instruções do 6502
- Todos os modos de endereçamento
- Comportamento das flags
- Timing das instruções
- Comportamento das interrupções

### Arquivos de Saída
- `valgrind.log`: Relatório de valgrind (se disponível)
- Arquivos `.o`: Objetos compilados
- `unit_tests`: Executável dos testes unitários
- `functional_test`: Executável do teste funcional

## Troubleshooting

### Problemas Comuns

1. **Erro de compilação**
   - Verifique se todas as dependências estão instaladas
   - Execute `make clean` antes de recompilar

2. **Teste funcional não encontrado**
   - Verifique se `6502_functional_test.bin` está no diretório `tests/`

3. **Valgrind não encontrado**
   - Instale o valgrind: `sudo apt-get install valgrind` (Ubuntu/Debian)
   - Os testes continuarão sem valgrind

4. **Testes falhando**
   - Verifique a implementação das instruções correspondentes
   - Use `make debug` para investigar com gdb

### Debugging

Para debugar um teste específico:
```bash
# Compilar com debug
make debug

# No gdb:
(gdb) break main
(gdb) run
(gdb) next
```

## Contribuindo

Para adicionar novos testes:

1. Adicione a função de teste em `unit_tests.c`
2. Chame a função em `main()`
3. Use as macros `TEST_ASSERT`, `TEST_ASSERT_EQUAL`, etc.
4. Execute `./run_tests.sh` para verificar

## Requisitos

- GCC ou Clang
- Make
- Bash (para o script de automação)
- Valgrind (opcional, para testes de memória)
- GDB (opcional, para debug)

## Licença

Este sistema de testes faz parte do emulador 6502 e segue a mesma licença do projeto principal. 