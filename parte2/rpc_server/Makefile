
# Makefile - Servidor RPC del servicio de registro de operaciones
#
# Compila el ejecutable log_rpc_server a partir de:
#   - log_rpc_server.c  : implementación de la lógica (escrito por nosotros)
#   - log_rpc_svc.c     : dispatcher generado por rpcgen
#   - log_rpc_xdr.c     : filtros XDR generados por rpcgen
#
# log_rpc_clnt.c NO se compila aquí (es para el cliente RPC, no el servidor)

CC = gcc
CFLAGS = -Wall -Wextra -g -Wno-unused-variable -Wno-unused-parameter -Wno-cast-function-type -I. $(shell pkg-config --cflags libtirpc 2>/dev/null)
LDFLAGS = -lnsl $(shell pkg-config --libs libtirpc 2>/dev/null)

TARGET = log_rpc_server

OBJ = log_rpc_server.o \
      log_rpc_svc.o \
      log_rpc_xdr.o

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: clean