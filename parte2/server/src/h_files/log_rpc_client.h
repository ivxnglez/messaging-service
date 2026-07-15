

#ifndef LOG_RPC_CLIENT_H
#define LOG_RPC_CLIENT_H

int log_rpc_init(void);

void log_rpc_destroy(void);

int log_rpc_send(const char *usuario, const char *operacion, const char *fichero);

#endif /* LOG_RPC_CLIENT_H */