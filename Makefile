# Compilador a ser utilizado
CC := gcc

# Opções de compilação
CFLAGS := -std=c11 -Wall -g -pthread -D_POSIX_C_SOURCE=200809L -O0 -I. -fsanitize=address -fsanitize=undefined -Werror -o -c

# Lista de arquivos .c a serem compilados
SRCS := main.c

# Lista de executáveis a serem gerados
TARGETS := $(SRCS:.c=)

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)