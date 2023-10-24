# Compilador a ser utilizado
CC := gcc

# Opções de compilação
CFLAGS := -std=c11 -Wall -g

# Lista de arquivos .c a serem compilados
SRCS := main.c

# Lista de executáveis a serem gerados
TARGETS := $(SRCS:.c=)

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)