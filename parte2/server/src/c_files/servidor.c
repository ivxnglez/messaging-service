
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "comunicaciones.h"
#include "funciones.h"
#include "database.h"

#include "log_rpc_client.h" // parte 2


// Sincronización entre hilo principal y trabajadores para el paso del descriptor
int sd_global;
int sd_principal; // para que se pueda cerrar el socket principal
int sd_copiado = 0;
pthread_mutex_t mutex_sd = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_sd  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_usuarios = PTHREAD_MUTEX_INITIALIZER;

// tamanio de los mensajes enviados del cliente
#define SHORT_MSG_SIZE 256


void *tratar_peticion(void *arg);

void manejador_sigint(int sig);



// funcion main que contiene el hilo principal
int main(int argc, char *argv[]) {

    int sd;
    int sd_conec;
    unsigned int tam_dir;
    struct sockaddr_in server_addr, client_addr;

    // parseamos los argumentos
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Uso: %s -p <puerto>\n", argv[0]);
        return -1;
    }

    // Convertimos el argumento del puerto a entero
    int puerto = atoi(argv[2]);

    // parte 2
    db_init();
    log_rpc_init();

    // Creación del socket TCP
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error al crear el socket.\n");
        return -1;
    }

    // ponemos esto para no tener problemas al reutilizar las dir ip
    int val = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(puerto);

    // Hacemos el bind
    if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error al hacer el bind.\n");
        close(sd);
        return -1;
    }

    // empezamos a escuchar
    if (listen(sd, SOMAXCONN) < 0) {
        fprintf(stderr, "Error al hacer el listen.\n");
        close(sd);
        return -1;
    }

    // mostramos init server <localIP>:<port>, y comenzamos definiendo variables necesarias
    char host[256];
    struct hostent *hp;
    char *ip_local;

    // obtenemos el nombre del host
    gethostname(host, sizeof(host));
    // obtenemos su direccion en binario
    hp = gethostbyname(host);
    // convertimos a formato texto (formato 0.0.0.0)
    ip_local = inet_ntoa(*(struct in_addr *)(hp->h_addr_list[0]));
    

    // mostramos init server <localIP>:<port> con el formato exacto del enunciado
    fprintf(stderr, "s> init server %s:%d\n", ip_local, puerto);
    fprintf(stderr, "s>\n");

    // Configuración de hilos detached
    pthread_attr_t t_attr;
    pthread_t hilo_id;
    pthread_attr_init(&t_attr);
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    tam_dir = sizeof(client_addr);

    sd_principal = sd; // Guardamos el socket maestro

    // para que cuando el usuario haga ctrl + c no haya problemas
    if (signal(SIGINT, manejador_sigint) == SIG_ERR) {
        fprintf(stderr, "Error al instalar el manejador de señales.\n");
        close(sd);
        return -1;
    }

    while (1) {
        // aceptamos la llamada del cliente y obtenemos el sd_conec (el especifico de la interaccion con el cliente)
        sd_conec = accept(sd, (struct sockaddr *)&client_addr, &tam_dir);

        if (sd_conec < 0) {
            fprintf(stderr, "Error al aceptar la peticion. Continuamos.\n");
            continue;
        }

        // copiamos el descriptor a sd_global tratando las condiciones de carrera
        pthread_mutex_lock(&mutex_sd);
        sd_copiado = 0;
        sd_global = sd_conec;

        // creamos el hilo que tratará la operación
        if (pthread_create(&hilo_id, &t_attr, tratar_peticion, NULL) != 0) {
            fprintf(stderr, "Error al crear el hilo. Continuamos.\n");
            close(sd_conec);
            pthread_mutex_unlock(&mutex_sd);
            continue;
        }

        // esperamos a que se copie el descriptor y una vez hecho, desbloqueamos el mutex para que pueda volver a escuchar mas peticiones
        // de otros clientes
        while (sd_copiado == 0) {
            pthread_cond_wait(&cond_sd, &mutex_sd);
        }
        pthread_mutex_unlock(&mutex_sd);
    }

    // cerramos todo
    close(sd);
    pthread_attr_destroy(&t_attr);
    return 0;
}




// hilo trabajador que se encarga de tratar la peticion del cliente
void *tratar_peticion(void *arg) {

    (void)arg;
    int s_local;
    char buffer_op[256];
    int bytes_leidos;

    // Copiamos el descriptor del socket bajo el mutex y avisamos al maestro
    pthread_mutex_lock(&mutex_sd);
    s_local = sd_global;
    sd_copiado = 1;
    pthread_cond_signal(&cond_sd);
    pthread_mutex_unlock(&mutex_sd);

    // una vez tenemos el descriptor copiado, llamamos a leer_cadena para saber qué operación vamos a realizar
    bytes_leidos = leer_cadena(s_local, buffer_op, 256);
    if (bytes_leidos <= 0) {
        close(s_local);
        pthread_exit(NULL);

        return NULL;
    }
    
    // comparamos lo que tenemos en buffer_op (la operacion a realizar), y para cada caso hacemos las llamadas extras pertinentes
    // si es REGISTER hacemos una llamada más
    if (strcmp(buffer_op, "REGISTER") == 0) {

        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // llamamos a la funcion f_register
        f_register(s_local, buffer_username);
        return NULL;
    }

    // si es UNREGISTER hacemos una llamada más
    else if (strcmp(buffer_op, "UNREGISTER") == 0) {
        
        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // llamamos a la funcion 
        f_unregister(s_local, buffer_username);
        return NULL;
    }

    // si es CONNECT hacemos dos llamadas más, username y el puerto de escucha
    else if (strcmp(buffer_op, "CONNECT") == 0) {
        
        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // hacemos otra para conocer el puerto de escucha
        char buffer_puerto[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_puerto, sizeof(buffer_puerto));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }
        int puerto_escucha = (int)strtol(buffer_puerto, NULL, 10);

        // llamamos a la funcion f_connect
        f_connect(s_local, buffer_username, puerto_escucha);
        return NULL;
    }

    // si es DISCONNECT hacemos hacemos una llamada más
    else if (strcmp(buffer_op, "DISCONNECT") == 0) {
        
        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // llamamos a la funcion 
        f_disconnect(s_local, buffer_username);
        return NULL;
    }

    // si es SEND, hacemos tres llamadas mas, una para username, otra para destinatario y otra para el mensaje
    else if (strcmp(buffer_op, "SEND") == 0) {
        
        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // hacemos otra llamada extra para conseguir el nombre del destinatario
        char buffer_destinatario[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_destinatario, sizeof(buffer_destinatario));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // hacemos otra para conocer el mensaje
        char buffer_message[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_message, sizeof(buffer_message));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // llamamos a la funcion 
        f_send(s_local, buffer_username, buffer_destinatario, buffer_message);
        return NULL;
    }

    // si es USERS, hacemos una llamada más
    else if (strcmp(buffer_op, "USERS") == 0) {
        
        // hacemos una llamada extra para conseguir el nombre del usuario
        char buffer_username[SHORT_MSG_SIZE];
        bytes_leidos = leer_cadena(s_local, buffer_username, sizeof(buffer_username));
        if (bytes_leidos <= 0) {
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }

        // llamamos a la funcion 
        f_users(s_local, buffer_username);
        return NULL;
    }

    // parte 2
    // si es SENDATTACH, hacemos cuatro llamadas mas, dos para los nombres y otras dos para el mensaje y el nombre del fichero
    else if (strcmp(buffer_op, "SENDATTACH") == 0) {
        char buf_user[SHORT_MSG_SIZE], buf_dest[SHORT_MSG_SIZE];
        char buf_msg[SHORT_MSG_SIZE],  buf_file[SHORT_MSG_SIZE];
        if (leer_cadena(s_local, buf_user, sizeof(buf_user)) <= 0) { 
            close(s_local); 
            pthread_exit(NULL);
            return NULL; 
        }
        if (leer_cadena(s_local, buf_dest, sizeof(buf_dest)) <= 0) { 
            close(s_local);
            pthread_exit(NULL);
            return NULL;
        }
        if (leer_cadena(s_local, buf_msg,  sizeof(buf_msg))  <= 0) { 
            close(s_local); 
            pthread_exit(NULL); 
            return NULL;
        }
        if (leer_cadena(s_local, buf_file, sizeof(buf_file)) <= 0) { 
            close(s_local); 
            pthread_exit(NULL); 
            return NULL;
        }
        f_sendattach(s_local, buf_user, buf_dest, buf_msg, buf_file);
        

        // una vez las funciones f_ terminen, cerramos el hilo y el socket 
        close(s_local);
        pthread_exit(NULL);

        return NULL;
    }

    return NULL;
}



// funcion que cierra de forma ordenada el socket cuando el usuario hace ctrl + c
void manejador_sigint(int sig) {
    (void)sig;
    fprintf(stderr, "\nApagando el servidor ordenadamente...\n");
    log_rpc_destroy();
    close(sd_principal);
    exit(0);
}



