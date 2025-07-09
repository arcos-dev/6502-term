# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I..
LDFLAGS = -lpthread

# Arquivos fonte para testes unitários
UNIT_SOURCES = unit_tests.c ../cpu_6502.c ../bus.c ../memory.c ../cpu_clock.c ../queue.c ../event_queue.c ../logging.c ../monitored.c
UNIT_OBJECTS = $(UNIT_SOURCES:.c=.o)

# Arquivos fonte para teste funcional
FUNC_SOURCES = functional_test.c ../cpu_6502.c ../bus.c ../memory.c ../cpu_clock.c ../queue.c ../event_queue.c ../logging.c ../monitored.c
FUNC_OBJECTS = $(FUNC_SOURCES:.c=.o)

# Objetos únicos (evitar duplicação)
COMMON_OBJECTS = ../cpu_6502.o ../bus.o ../memory.o ../cpu_clock.o ../queue.o ../event_queue.o ../logging.o ../monitored.o

# Nomes dos executáveis
UNIT_TARGET = unit_tests
FUNC_TARGET = functional_test

# Regra padrão
all: $(UNIT_TARGET) $(FUNC_TARGET)

# Compilar testes unitários
$(UNIT_TARGET): unit_tests.o $(COMMON_OBJECTS)
	$(CC) unit_tests.o $(COMMON_OBJECTS) -o $(UNIT_TARGET) $(LDFLAGS)

# Compilar teste funcional
$(FUNC_TARGET): functional_test.o $(COMMON_OBJECTS)
	$(CC) functional_test.o $(COMMON_OBJECTS) -o $(FUNC_TARGET) $(LDFLAGS)

# Regra para compilar arquivos .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Executar testes unitários
test: $(UNIT_TARGET)
	./$(UNIT_TARGET)

# Executar teste funcional
run_functional_test: $(FUNC_TARGET)
	./$(FUNC_TARGET)

# Limpar arquivos compilados
clean:
	rm -f unit_tests.o functional_test.o $(COMMON_OBJECTS) $(UNIT_TARGET) $(FUNC_TARGET)

# Executar com valgrind (se disponível)
valgrind: $(UNIT_TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(UNIT_TARGET)

# Executar com gdb
debug: $(UNIT_TARGET)
	gdb ./$(UNIT_TARGET)

# Mostrar ajuda
help:
	@echo "Comandos disponíveis:"
	@echo "  make              - Compilar todos os testes"
	@echo "  make test         - Compilar e executar testes unitários"
	@echo "  make run_functional_test - Compilar e executar teste funcional"
	@echo "  make clean        - Limpar arquivos compilados"
	@echo "  make valgrind     - Executar com valgrind (detecção de vazamentos)"
	@echo "  make debug        - Executar com gdb para debug"

.PHONY: all test clean valgrind debug help run_functional_test
