# Makefile - Servidor de mensajería (Parte 1 + Parte 2)
#
# Compila el ejecutable server a partir de:
#   - servidor.c        : hilo principal y dispatcher de operaciones
#   - funciones.c       : lógica de cada operación (register, send, etc.)
#   - database.c        : base de datos en ficheros
#   - comunicaciones.c  : funciones de envío/recepción de mensajes por socket
#   - log_rpc_client.c  : cliente RPC que llama al servidor de log (Parte 2)
#   - log_rpc_clnt.c    : stub cliente generado por rpcgen
#   - log_rpc_xdr.c     : filtros XDR generados por rpcgen
#
# IMPORTANTE: antes de compilar hay que haber ejecutado dentro de src/rpc/:
#     rpcgen -aNM log_rpc.x

CC = gcc
CFLAGS =-Wall -Wextra -g -Wno-unused-variable -I./src/h_files -I./src/rpc \
         $(shell pkg-config --cflags libtirpc 2>/dev/null)
LDFLAGS = -lpthread -lnsl $(shell pkg-config --libs libtirpc 2>/dev/null)

TARGET = server

OBJ = src/c_files/servidor.o \
      src/c_files/funciones.o \
      src/c_files/database.o \
      src/c_files/comunicaciones.o \
      src/c_files/log_rpc_client.o \
      src/rpc/log_rpc_clnt.o \
      src/rpc/log_rpc_xdr.o

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: clean