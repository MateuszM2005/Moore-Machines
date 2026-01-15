# Kompilator
CC = gcc

# Flagi kompilacji dla biblioteki
CFLAGS = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2

# Flagi linkera dla biblioteki
LDFLAGS = -shared \
  -Wl,--wrap=malloc \
  -Wl,--wrap=calloc \
  -Wl,--wrap=realloc \
  -Wl,--wrap=reallocarray \
  -Wl,--wrap=free \
  -Wl,--wrap=strdup \
  -Wl,--wrap=strndup

# Nazwa wynikowej biblioteki
LIB = libma.so

# Pliki źródłowe do biblioteki
LIB_SRC = ma.c memory_tests.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB_DEP = $(LIB_SRC:.c=.d)

# Domyślny cel
all: $(LIB)

# Cel do stworzenia biblioteki współdzielonej
$(LIB): $(LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Reguła ogólna do kompilacji .c do .o i generowania zależności .d
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# Wczytywanie zależności
-include $(LIB_DEP) 

# Czyszczenie plików wynikowych
clean:
	rm -f *.o *.d *.so 

.PHONY: all clean
