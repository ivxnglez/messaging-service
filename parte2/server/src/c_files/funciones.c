// aqui iran las funciones register, send, unregister ...

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "comunicaciones.h"
#include "database.h"

#include "log_rpc_client.h" // Parte 2

// Mutex de la base de datos (definido en database.c) para sincronizar lecturas
extern pthread_mutex_t db_mutex;



// funcion auxiliar para obtener los datos de un usuario
static int get_user_info(const char *userName, int *connected, char *ip, int *port) {
    
    // abrimos usuarios.txt y controlamos la concurrencia con el db_mutex
    pthread_mutex_lock(&db_mutex);
    FILE *f = fopen("database/usuarios.txt", "r");
    if (!f) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }
    
    char name[256], read_ip[20];
    int read_conn, read_port;
    unsigned int last_id;
    
    // leemos linea por linea
    while (fscanf(f, "%255s %d %19s %d %u", name, &read_conn, read_ip, &read_port, &last_id) == 5) {
        
        // si encontramos el usuario, copiamos sus datos 
        if (strcmp(name, userName) == 0) {
            *connected = read_conn;
            strcpy(ip, read_ip);
            *port = read_port;
            fclose(f);
            pthread_mutex_unlock(&db_mutex);
            return 0;
        }
    }
    
    fclose(f);
    pthread_mutex_unlock(&db_mutex);
    return -1;
}



// funcion para registar a un usuario en usuarios.txt
void f_register(int sd, char* username){

    // metemos al usuario en la base de datos llamando a db_regiser
    int resultado = db_register(username);

    // imprimimos el resultado en el servidor
    if (resultado == 0) {
        fprintf(stderr,"s> REGISTER %s OK\n", username);
    } else {
        fprintf(stderr,"s> REGISTER %s FAIL\n", username);
    }

    // devolvemos al cliente el resultado obtenido (puede ser 0, 1 o 2)
    // Convertimos el resultado (int) a un byte (char)
    char respuesta = (char)resultado;

    // Le pasamos a sendMessage la dirección de memoria de 'respuesta' usando '&'
    // Y le indicamos que la longitud a enviar es de 1 byte
    sendMessage(sd, &respuesta, 1);

    // Parte 2
    log_rpc_send(username, "REGISTER", "");
}



// funcion para eliminar a un usuario en usuarios.txt
void f_unregister(int sd, char* username){

    // metemos al usuario en la base de datos llamando a db_unregiser
    int resultado = db_unregister(username);

    // imprimimos el resultado en el servidor
    if (resultado == 0) {
        fprintf(stderr,"s> UNREGISTER %s OK\n", username);
    } else {
        fprintf(stderr,"s> UNREGISTER %s FAIL\n", username);
    }

    // devolvemos al cliente el resultado obtenido (puede ser 0, 1 o 2)
    // Convertimos el resultado (int) a un byte (char)
    char respuesta = (char)resultado;

    // Le pasamos a sendMessage la dirección de memoria de 'respuesta' usando '&'
    // Y le indicamos que la longitud a enviar es de 1 byte
    sendMessage(sd, &respuesta, 1);

    // parte 2, hacemos un send al rpc
    log_rpc_send(username, "UNREGISTER", "");
}



// funcion para 'conectar' a un usuario
void f_connect(int sd, char* username, int puerto_escucha) {

    char response_code;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char ip_str[INET_ADDRSTRLEN];

    // obtenemos la IP del cliente y la copiamos a ip_str
    if (getpeername(sd, (struct sockaddr*)&client_addr, &addr_len) == -1) {
        response_code = 3;
        sendMessage(sd, &response_code, 1);
        fprintf(stderr,"s> CONNECT %s FAIL\n", username);
        return;
    }
    strcpy(ip_str, inet_ntoa(client_addr.sin_addr));

    // le conectamos a usuarios.txt con db_connect
    int res = db_connect(username, ip_str, puerto_escucha);
    response_code = (char)res;
    
    // enviamos respuesta al cliente
    sendMessage(sd, &response_code, 1);

    // parte 2: hacemos un rpc send
    log_rpc_send(username, "CONNECT", "");

    // si ha habido exito, enviamos los mensajes pendientes
    if (res == 0) {
        fprintf(stderr,"s> CONNECT %s OK\n", username);
        
        // llamamos a db_get_pending_messages para obtener sus mensajes pendientes
        PendingMsg *msgs = NULL;
        int numMsgs = 0;
        db_get_pending_messages(username, &msgs, &numMsgs);
        
        // mandamos cada mensaje que tuviese pendiente el ususario (una iteracion por mensaje)
        for (int i = 0; i < numMsgs; i++) {

            // Nos conectamos al hilo con el socket de escucha del cliente recién conectado
            int sd_dest = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in dest_addr;
            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(puerto_escucha);
            inet_aton(ip_str, &dest_addr.sin_addr);

            // Si hemos conseguido su direccion de forma exitosa, mandamos el mensaje
            if (sd_dest >= 0 && connect(sd_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {
                char id_str[32];
                sprintf(id_str, "%u", msgs[i].id);
                // Parte 2: obtenemos si tiene algo asociado
                int has_attach = (msgs[i].fileName[0] != '\0');
                
                // enviamos cada parte del mensaje según su protocolo y cerramos la conexion
                // parte2: Ahora nos fijamos si tiene o no un filename asociado, y mandamos los mensajes pertinentes
                if (has_attach) {
                    sendMessage(sd_dest, "SEND_MESSAGE_ATTACH\0", 20);
                    sendMessage(sd_dest, msgs[i].sender, strlen(msgs[i].sender) + 1);
                    sendMessage(sd_dest, id_str, strlen(id_str) + 1);
                    sendMessage(sd_dest, msgs[i].message, strlen(msgs[i].message) + 1);
                    sendMessage(sd_dest, msgs[i].fileName, strlen(msgs[i].fileName) + 1);
                } else {
                    sendMessage(sd_dest, "SEND_MESSAGE\0", 13);
                    sendMessage(sd_dest, msgs[i].sender, strlen(msgs[i].sender) + 1);
                    sendMessage(sd_dest, id_str, strlen(id_str) + 1);
                    sendMessage(sd_dest, msgs[i].message, strlen(msgs[i].message) + 1);
                }
                close(sd_dest);

                // imprimimos en la terminal el mensaje de exito
                fprintf(stderr,"s> SEND MESSAGE %u FROM %s TO %s\n", msgs[i].id, msgs[i].sender, username);

                // borramos de la lista de mensajes pendientes del usuario el mensaje anterior
                db_delete_message(username, msgs[i].id);

                // si el remitente esta conectado, mandamos el ack confirmando que el usuario recibio su mensaje
                // llamamos a get_user_info para obtener los datos del remitente
                int s_conn; char s_ip[20]; int s_port;
                if (get_user_info(msgs[i].sender, &s_conn, s_ip, &s_port) == 0 && s_conn == 1) {
                    int sd_src = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in src_addr;
                    memset(&src_addr, 0, sizeof(src_addr));
                    src_addr.sin_family = AF_INET;
                    src_addr.sin_port   = htons(s_port);
                    inet_aton(s_ip, &src_addr.sin_addr);

                    // mandamos la confirmacion de que su mensaje llegó al usuario
                    if (sd_src >= 0 && connect(sd_src, (struct sockaddr *)&src_addr, sizeof(src_addr)) == 0) {
                        
                        // parte2: dependiendo de nuevo de si tiene filename mandamos un numero de mensajes u otro
                        if (has_attach) {
                            sendMessage(sd_src, "SEND_MESS_ATTACH_ACK\0", 21);
                            sendMessage(sd_src, id_str, strlen(id_str) + 1);
                            sendMessage(sd_src, msgs[i].fileName, strlen(msgs[i].fileName) + 1);
                        } else {
                            sendMessage(sd_src, "SEND_MESS_ACK\0", 14);
                            sendMessage(sd_src, id_str, strlen(id_str) + 1);
                        }
                        close(sd_src);
                        
                    } else {
                        if (sd_src >= 0) close(sd_src);
                        db_disconnect(msgs[i].sender); 
                    }
                }
            } 
            
            // Si no, mostramos un fallo
            else {
                if (sd_dest >= 0) close(sd_dest);
                db_disconnect(username);
                break; // Cortamos la entrega
            }
        }
        if (msgs) free(msgs);
    } else {
        fprintf(stderr,"s> CONNECT %s FAIL\n", username);
    }
}



// funcion para desconectar a un usuario
void f_disconnect(int sd, char* username) {
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char ip_cliente[INET_ADDRSTRLEN];

    // obtenemos la IP del cliente que hace la petición
    if (getpeername(sd, (struct sockaddr*)&client_addr, &addr_len) == -1) {
        char code = 3;
        sendMessage(sd, &code, 1);
        fprintf(stderr,"s> DISCONNECT %s FAIL\n", username);
        log_rpc_send(username, "DISCONNECT", "");
        return;
    }
    strcpy(ip_cliente, inet_ntoa(client_addr.sin_addr));

    // comprobamos la IP registrada para este usuario
    int connected; char ip_registrada[20]; int port;
    int info_res = get_user_info(username, &connected, ip_registrada, &port);

    // Si el usuario no existe
    if (info_res != 0) {
        
        char code = 1;
        sendMessage(sd, &code, 1);
        fprintf(stderr,"s> DISCONNECT %s FAIL\n", username);
        log_rpc_send(username, "DISCONNECT", "");
        return;
    }

    // comprobamos que la ip del cliente que se quiere desconectar sea la misma que la del usuario que intenta desconectar
    if (strcmp(ip_cliente, ip_registrada) != 0) {
        char code = 3;
        sendMessage(sd, &code, 1);
        fprintf(stderr,"s> DISCONNECT %s FAIL\n", username);
        log_rpc_send(username, "DISCONNECT", "");
        return;
    }

    // si la IP coincide, lo desconenctamos de usuarios.txt y mandamos el mensaje de respuesta
    int res = db_disconnect(username);

    char response_code = (char)res;
    sendMessage(sd, &response_code, 1);

    // imprimimos por terminal lo pertinente
    if (res == 0) {
        fprintf(stderr,"s> DISCONNECT %s OK\n", username);
    } else {
        fprintf(stderr,"s> DISCONNECT %s FAIL\n", username);
    }

    log_rpc_send(username, "DISCONNECT", "");
}



// funcion para imprimir la lsita de usuarios conectados
void f_users(int sd, char* username) {
    char **usersList = NULL;
    int numUsers = 0;
    
    // llamamos a db_get_connected_users para obtener los usuarios conectados
    int res = db_get_connected_users(username, &usersList, &numUsers);

    // mandamos el resultado al usuario
    char response_code = (char)res;
    sendMessage(sd, &response_code, 1);

    // Si ha habido exito, mandamos un mensaje por usuario conectado
    if (res == 0) {
        
        // mandamos el numero de usuarios conectados
        char count_str[16];
        sprintf(count_str, "%d", numUsers);
        sendMessage(sd, count_str, strlen(count_str) + 1);

        // enviamos cada usuario conectado
        for (int i = 0; i < numUsers; i++) {
            sendMessage(sd, usersList[i], strlen(usersList[i]) + 1);
        }

        // parte2
        log_rpc_send(username, "USERS", "");

        fprintf(stderr,"s> CONNECTEDUSERS OK\n");
        db_free_users_list(usersList, numUsers);

    } else {
        fprintf(stderr,"s> CONNECTEDUSERS FAIL\n");
    }
}



// funcion que manda un mensaje entre dos usuarios
void f_send(int sd, char* username, char* destinatario, char* message) {
    
    unsigned int msg_id;
    int recv_connected;
    char recv_ip[20];
    int recv_port;
    
    // preparamos el mensaje con db_prepare_message
    int res = db_prepare_message(username, destinatario, &msg_id, &recv_connected, recv_ip, &recv_port);
    char response_code = (char)res;
    
    // Envia código inicial
    sendMessage(sd, &response_code, 1);

    if (res != 0) {
        fprintf(stderr,"s> SEND FAIL\n");
        log_rpc_send(username, "SEND", "");
        return; // Error (código 1 o 2)
    }

    // enviamos el id del mensaje al usuario
    char id_str[32];
    sprintf(id_str, "%u", msg_id);
    sendMessage(sd, id_str, strlen(id_str) + 1);

    // parte 2
    log_rpc_send(username, "SEND", "");

    // guardamos el mensaje en la lista de mensajes del destinatario
    db_store_message(destinatario, username, msg_id, message, NULL);

    // si el destinatario esta conectado, le mandamos directamente el mensaje
    if (recv_connected == 1) {

        // creamos el socket que conectará con el destinarario
        int sd_dest = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(recv_port);
        inet_aton(recv_ip, &dest_addr.sin_addr);

        // intentamos conectarnos al destinatario, si tiene exito, le mandamos las partes del mensaje según el protocolo
        if (sd_dest >= 0 && connect(sd_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {
            sendMessage(sd_dest, "SEND_MESSAGE\0", 13);
            sendMessage(sd_dest, username, strlen(username) + 1);
            sendMessage(sd_dest, id_str, strlen(id_str) + 1);
            sendMessage(sd_dest, message, strlen(message) + 1);
            close(sd_dest);

            // mostramos por el servidor que se ha enviado el mensaje
            fprintf(stderr,"s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, username, destinatario);
            
            // Borrar del fichero de mensajes pendientes del destinatario el mensaje entregado
            db_delete_message(destinatario, msg_id);

            // si el remitente sigue conectado, le mandamos el ack
            // obtenemos la informacion del usuario para mandarle el mensaje
            int s_conn; char s_ip[20]; int s_port;
            if (get_user_info(username, &s_conn, s_ip, &s_port) == 0 && s_conn == 1) {
                
                // creamos un nuevo socket (el anterior lo cerramos arriba)
                int sd_src = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in src_addr;
                memset(&src_addr, 0, sizeof(src_addr));
                src_addr.sin_family = AF_INET;
                src_addr.sin_port = htons(s_port);
                inet_aton(s_ip, &src_addr.sin_addr);

                // le mandamos el ack al usuario mediante dos sends, uno indicando la operacion y otro con el ack
                if (sd_src >= 0 && connect(sd_src, (struct sockaddr *)&src_addr, sizeof(src_addr)) == 0) {
                    sendMessage(sd_src, "SEND_MESS_ACK\0", 14);
                    sendMessage(sd_src, id_str, strlen(id_str) + 1);
                    close(sd_src);
                } else {
                    if (sd_src >= 0) close(sd_src);
                    db_disconnect(username); 
                }
            }
        } 
        // Si no nos podemos conectar, simplemente nos desconectamos e imprimos en la terminal lo de abajo
        else {
            
            if (sd_dest >= 0) close(sd_dest);
            db_disconnect(destinatario);
            fprintf(stderr,"s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, username, destinatario);
        }
    } 
    // Si no está conectado, imprimimos lo de abajo
    else {
        fprintf(stderr,"s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, username, destinatario);
    }
}




// parte 2: funcion sendattach. lo mismo que el send pero con el nombre del fichero tambien
void f_sendattach(int sd, char* username, char* destinatario, char* message, char* fileName) {
    
    unsigned int msg_id;
    int recv_connected;
    char recv_ip[20];
    int recv_port;

    // preparamos el mensaje
    int res = db_prepare_message(username, destinatario, &msg_id, &recv_connected, recv_ip, &recv_port);
    
    // mandamos la respuesta de la funcion anterior
    char response_code = (char)res;
    sendMessage(sd, &response_code, 1);
    if (res != 0) {
        fprintf(stderr,"s> SENDATTACH FAIL\n");
        log_rpc_send(username, "SENDATTACH", fileName);
        return;
    }

    // enviamos el id del mensaje al usuario
    char id_str[32];
    sprintf(id_str, "%u", msg_id);
    sendMessage(sd, id_str, strlen(id_str) + 1);
    log_rpc_send(username, "SENDATTACH", fileName);

    // guardamos el mensaje en la base de datos (en el fichero de los mensajes de destinatario)
    db_store_message(destinatario, username, msg_id, message, fileName);

    // si el destinatario está conectado, le enviamos el mensaje directamente
    if (recv_connected == 1) {

        // nos conectamos al socket del destinatario (el de escucha del cliente)
        int sd_dest = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port   = htons(recv_port);
        inet_aton(recv_ip, &dest_addr.sin_addr);

        // intentamos conectarnos al destinatario, si tiene exito, le mandamos las partes del mensaje según el protocolo
        if (sd_dest >= 0 && connect(sd_dest, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == 0) {
            sendMessage(sd_dest, "SEND_MESSAGE_ATTACH\0", 20);
            sendMessage(sd_dest, username, strlen(username) + 1);
            sendMessage(sd_dest, id_str, strlen(id_str) + 1);
            sendMessage(sd_dest, message, strlen(message) + 1);
            sendMessage(sd_dest, fileName, strlen(fileName) + 1);
            close(sd_dest);

            // mostramos por el servidor que se ha enviado el mensaje
            fprintf(stderr,"s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, username, destinatario);

            // Borrar del fichero de mensajes pendientes del destinatario el mensaje entregado
            db_delete_message(destinatario, msg_id);

            // si el remitente sigue conectado, le mandamos el ack
            // obtenemos la informacion del usuario para mandarle el mensaje
            int s_conn; char s_ip[20]; int s_port;
            if (get_user_info(username, &s_conn, s_ip, &s_port) == 0 && s_conn == 1) {

                // creamos un nuevo socket (el anterior lo cerramos arriba)
                int sd_src = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in src_addr;
                memset(&src_addr, 0, sizeof(src_addr));
                src_addr.sin_family = AF_INET;
                src_addr.sin_port   = htons(s_port);
                inet_aton(s_ip, &src_addr.sin_addr);

                // le mandamos el ack al usuario mediante dos sends, uno indicando la operacion y otro con el ack
                if (sd_src >= 0 && connect(sd_src, (struct sockaddr *)&src_addr, sizeof(src_addr)) == 0) {
                    sendMessage(sd_src, "SEND_MESS_ATTACH_ACK\0", 21);
                    sendMessage(sd_src, id_str, strlen(id_str) + 1);
                    sendMessage(sd_src, fileName, strlen(fileName) + 1);
                    close(sd_src);
                } else {
                    if (sd_src >= 0) close(sd_src);
                    db_disconnect(username);
                }
            }
        } 
        
        // Si no nos podemos conectar, simplemente nos desconectamos e imprimos en la terminal lo de abajo
        else {
            if (sd_dest >= 0) close(sd_dest);
            db_disconnect(destinatario);
            fprintf(stderr,"s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, username, destinatario);
        }
    } 
    
    // Si no está conectado, imprimimos lo de abajo
    else {
        fprintf(stderr,"s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, username, destinatario);
    }
}



