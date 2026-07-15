

#include "log_rpc_client.h"
#include "log_rpc.h"       

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <rpc/rpc.h>


// Cliente RPC compartido entre todos los hilos del servidor.
static CLIENT *clnt = NULL;

// mutex que serializa las llamadas RPC entre hilos 
static pthread_mutex_t rpc_mutex = PTHREAD_MUTEX_INITIALIZER;

// Flag que indica si el log RPC está habilitado 
static int enabled = 0;


// inicializa el cliente RPC leyendo LOG_RPC_IP.
int log_rpc_init(void)
{
    // inicalizamos el log cogiendo la variable de entorno LOG_RPC_IP
    char *host = getenv("LOG_RPC_IP");
    if (host == NULL) {
        fprintf(stderr, "AVISO: LOG_RPC_IP no definida. Log RPC deshabilitado.\n");
        enabled = 0;
        return -1;
    }

    // usamos TCP para garantizar fiabilidad 
    clnt = clnt_create(host, LOG_RPC, LOG_RPC_VERSION, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(host);
        fprintf(stderr, "AVISO: no se pudo conectar al servidor RPC en %s. Log deshabilitado.\n", host);
        enabled = 0;
        return -1;
    }

    enabled = 1;
    fprintf(stderr, "Cliente RPC de log conectado a %s.\n", host);
    return 0;
}


// destruye el cliente RPC y libera recursos.
void log_rpc_destroy(void)
{
    pthread_mutex_lock(&rpc_mutex);
    if (clnt != NULL) {
        clnt_destroy(clnt);
        clnt = NULL;
    }
    enabled = 0;
    pthread_mutex_unlock(&rpc_mutex);
}


// Envía una operación al servidor RPC.
int log_rpc_send(const char *usuario, const char *operacion, const char *fichero)
{
    if (!enabled || clnt == NULL)
        return 0;

    if (usuario == NULL || operacion == NULL)
        return -1;

    // si fichero es NULL lo sustituimos por cadena vacía para no pasarle NULL al stub XDR, que podría crashear 
    const char *fich = (fichero != NULL) ? fichero : "";

    // Copiamos a buffers locales porque el stub generado por rpcgen espera char* 
    char buf_usr[260];
    char buf_op[260];
    char buf_fich[260];
    strncpy(buf_usr,  usuario,   255); buf_usr[255]  = '\0';
    strncpy(buf_op,   operacion, 255); buf_op[255]   = '\0';
    strncpy(buf_fich, fich,      255); buf_fich[255] = '\0';

    int result = -1;
    pthread_mutex_lock(&rpc_mutex);

    enum clnt_stat stat = log_operation_1(buf_usr, buf_op, buf_fich, &result, clnt);

    pthread_mutex_unlock(&rpc_mutex);

    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "Error en RPC log_operation");
        return -1;
    }

    return result;
}

