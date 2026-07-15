
# Variables del compilador y flags
CC = gcc
# -Wall: muestra todos los warnings (requisito del enunciado)
# -I: indica el directorio donde están los archivos .h[cite: 19]
# -lpthread: librería necesaria para servidores multihilo
CFLAGS = -Wall -Wextra -g -I./src/h_files
LDFLAGS = -lpthread

# Nombre del ejecutable final[cite: 2, 11]
TARGET = server

# Lista de archivos objeto necesarios para el servidor
OBJ = src/c_files/servidor.o \
      src/c_files/funciones.o \
      src/c_files/database.o \
      src/c_files/comunicaciones.o

# Regla principal: compila el ejecutable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

# Regla para compilar archivos .c a .o de forma genérica
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar los archivos generados
clean:
	rm -f $(TARGET) $(OBJ)