
#include "database.h"
#include <stdio.h>       // Solo para snprintf y sscanf (manejo de strings en memoria, no ficheros)
#include <stdlib.h>      // Para malloc y free
#include <string.h>      // Para strcmp, strcpy
#include <pthread.h>     // Para mutex
#include <unistd.h>      // Para llamadas al sistema: read, write, close, unlink, lseek
#include <fcntl.h>       // Para flags de open: O_RDONLY, O_WRONLY, O_CREAT, etc.
#include <sys/stat.h>    // Para mkdir y permisos

// Mutex global para serializar el acceso a la base de datos
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

// Rutas de los ficheros
const char *DB_DIR = "database";
const char *DB_USERS_FILE = "database/usuarios.txt";
const char *DB_TEMP_FILE = "database/temp_usuarios.txt";


// funcion para leer cada linea de los ficheros
ssize_t read_line_sys(int fd, char *buffer, size_t max_len) {
    size_t i = 0;              
    char c;                    
    ssize_t bytes_read;        

    // vamos leyendo byte a byte
    while (i < max_len - 1) {
        bytes_read = read(fd, &c, 1);
        
        if (bytes_read > 0) {

            // guardamos el byte en nuestro buffer y avanzamos el índice
            buffer[i++] = c;

            // si es un salto de línea, terminamos de leer la línea
            if (c == '\n') break;

        } else if (bytes_read == 0) {

            // si devuelve 0, es que se acabó el archivo
            break; 

        } else {

            // si devuelve -1, hubo un error de lectura
            return -1; 
        }
    }
    // Convertimos el buffer en una cadena válida de C poniendo el terminador nulo
    buffer[i] = '\0';
    // devolvemos el total de bytes leídos
    return i; 
}


int db_init() {
    pthread_mutex_lock(&db_mutex);
    
    struct stat st = {0};
    // Comprobamos si el directorio existe
    if (stat(DB_DIR, &st) == -1) {
        // Si no existe, usamos mkdir (llamada al sistema) con permisos 0700
        mkdir(DB_DIR, 0700);
    }

    // si usuarios.txt no exite, lo creamos
    int fd = open(DB_USERS_FILE, O_CREAT | O_APPEND | O_WRONLY, 0644);
    
    // Si la llamada no dio error (fd >= 0), cerramos el descriptor.
    if (fd >= 0) close(fd);
    
    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion que inserta un usuario en usuarios.txt
int db_register(const char *userName) {
    pthread_mutex_lock(&db_mutex);
    
    //abrimos el fichero usuarios.txt
    int fd = open(DB_USERS_FILE, O_RDONLY);
    char line_buf[512]; // Buffer de memoria temporal para leer las líneas
    
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;

    // si el fichero existe, verficamos que no esté el usuario ya registrado
    if (fd >= 0) {
        while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {

            // Comprobamos linea por linea
            if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {

                // Si el usuario ya está registrado, cerramos todo
                if (strcmp(name, userName) == 0) {
                    close(fd); // Cerramos descriptor
                    pthread_mutex_unlock(&db_mutex);
                    return 1; 
                }
            }
        }
        close(fd);
    }

    // Si no existe, lo creamos
    fd = open(DB_USERS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&db_mutex);
        return 2;
    }
    
    // Copiamos a line_buf la nueva linea a insertar
    int len = snprintf(line_buf, sizeof(line_buf), "%s 0 0.0.0.0 0 0\n", userName);
    
    // Registramos al usuario en la ultima linea de usuarios.txt
    write(fd, line_buf, len);
    
    close(fd);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion para eliminar de usuarios.txt a un usuario
int db_unregister(const char *userName) {
    pthread_mutex_lock(&db_mutex);
    
    // Si usuarios.txt no existe, retornamos un error
    int fd = open(DB_USERS_FILE, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&db_mutex); return 1; }

    // creamos el fichero temporal (o lo vaciamos si ya estaba creado)
    int fd_temp = open(DB_TEMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_temp < 0) { close(fd); pthread_mutex_unlock(&db_mutex); return 2; }

    char line_buf[512];
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;
    int found = 0;

    // leemos usuarios.txt linea a linea
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {

        // vamos copiando linea a linea de usuarios.txt al fichero temporal
        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            
            // si encontramos el usuarios, nos lo saltamos y no lo copiamos
            if (strcmp(name, userName) == 0) {
                found = 1; 

            } else {
                int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
                write(fd_temp, line_buf, len);
            }
        }
    }

    close(fd);
    close(fd_temp);

    // si encontramos el usuario
    if (found) {

        // eliminamos usuarios.txt y renombramos el fichero temporal a usuarios.txt
        unlink(DB_USERS_FILE);
        rename(DB_TEMP_FILE, DB_USERS_FILE);
        
        // Borramos su fichero de los mensajes pendientes
        char msg_file[300];
        snprintf(msg_file, sizeof(msg_file), "database/mensajes_%s.dat", userName);
        unlink(msg_file); 
        
        pthread_mutex_unlock(&db_mutex);
        return 0;
    } 
    
    // Si no encontramos al usuario, retornamos un 1, eliminamos el temp y dejamos usuarios.txt como esta
    else {
        unlink(DB_TEMP_FILE); 
        pthread_mutex_unlock(&db_mutex);
        return 1;
    }
}


// funcion que cambia el campo conectado de un usuario en usuarios.txt
int db_connect(const char *userName, const char *ip_in, int port_in) {
    pthread_mutex_lock(&db_mutex);
    
    // abrimos usuarios.txt
    int fd = open(DB_USERS_FILE, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&db_mutex); return 1; }

    // abrimos el fichero temporal o lo creamos si no existe
    int fd_temp = open(DB_TEMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_temp < 0) { close(fd); pthread_mutex_unlock(&db_mutex); return 3; }

    char line_buf[512];
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;
    int status = 1; 

    // vamos leyendo linea a linea
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {

        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            
            // Si lo encontramos, miramos el campo connected
            if (strcmp(name, userName) == 0) {

                // Si ya lo estaba, copiamos esta línea tambien y no cambiamos nada
                if (connected == 1) {
                    status = 2; // Devolveremos que ya está conectado
                    int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
                    write(fd_temp, line_buf, len);

                // Si no estaba conectado, cambiamos el campo connected y copiamos la linea
                } else {
                    status = 0; // Devolveremos que todo ha ido bien
                    int len = snprintf(line_buf, sizeof(line_buf), "%s 1 %s %d %u\n", name, ip_in, port_in, last_id);
                    write(fd_temp, line_buf, len);
                }
            
            // Si no lo hemos encontrado, no hacemos nada y copiamos la linea
            } else {
                int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
                write(fd_temp, line_buf, len);
            }
        }
    }
    close(fd);
    close(fd_temp);

    // dependiendo del status, eliminamos usuarios.txt o no
    if (status == 0) {
        unlink(DB_USERS_FILE);
        rename(DB_TEMP_FILE, DB_USERS_FILE);
    } else {
        unlink(DB_TEMP_FILE);
    }
    
    pthread_mutex_unlock(&db_mutex);
    return status;
}



// funcion para desconectar al usuario en usuarios.txt
int db_disconnect(const char *userName) {
    pthread_mutex_lock(&db_mutex);
    
    // abrimos usuarios.txt
    int fd = open(DB_USERS_FILE, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&db_mutex); return 1; }

    // abrimos el fichero temporal
    int fd_temp = open(DB_TEMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_temp < 0) { close(fd); pthread_mutex_unlock(&db_mutex); return 3; }

    char line_buf[512];
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;
    int status = 1; 

    // leemos linea a linea
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {
        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            
            // si encontramos el usuario, miramos connected
            if (strcmp(name, userName) == 0) {

                // Si estaba desconectado, no hacemos nada y copiamos su linea
                if (connected == 0) {
                    status = 2; 
                    int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
                    write(fd_temp, line_buf, len);

                // Si no lo estaba, cambiamos el campo connected y copiamos la linea a temp
                } else {
                    status = 0; // Ok
                    int len = snprintf(line_buf, sizeof(line_buf), "%s 0 0.0.0.0 0 %u\n", name, last_id);
                    write(fd_temp, line_buf, len);
                }

            // Si esta linea no es la del usuario, simplemente la copiamos a temp
            } else {
                int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
                write(fd_temp, line_buf, len);
            }
        }
    }
    close(fd);
    close(fd_temp);

    // dependiendo del status, eliminamos usuarios.txt o no
    if (status == 0) {
        unlink(DB_USERS_FILE);
        rename(DB_TEMP_FILE, DB_USERS_FILE);
    } else {
        unlink(DB_TEMP_FILE);
    }
    
    pthread_mutex_unlock(&db_mutex);
    return status;
}

// funcion que comprueba si los usuarios existen y estan conectados e incremente el id del ultimo mensaje envidado en usuarios
int db_prepare_message(const char *sender, const char *receiver, unsigned int *msg_id, int *recv_connected, char *recv_ip, int *recv_port) {
    pthread_mutex_lock(&db_mutex);
    
    // abre usuarios.txt
    int fd = open(DB_USERS_FILE, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&db_mutex); return 2; }

    int sender_exists = 0, receiver_exists = 0;
    char line_buf[512];
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;

    // verifica linea a linea si el receptor y el destinatario existen
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {
        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            if (strcmp(name, sender) == 0) sender_exists = 1;
            if (strcmp(name, receiver) == 0) receiver_exists = 1;
        }
    }
    
    // si no existe alguno de los dos retornamos un error de que no existen
    if (!sender_exists || !receiver_exists) {
        close(fd);
        pthread_mutex_unlock(&db_mutex);
        return 1; 
    }

    // volvemos al principio del todo y creamos un abrimos (o creamos) el fichero temporal
    lseek(fd, 0, SEEK_SET);
    int fd_temp = open(DB_TEMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    // Hacemos uns segund pasada para incrementar el valor del id y guardar los valores
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {
        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            
            // si es el remitente, incrementamos el valor de last_id y lo copiamos a msg_id
            if (strcmp(name, sender) == 0) {
                if (last_id == 4294967295U) last_id = 0;
                last_id++;
                *msg_id = last_id; 
            }

            // Si es el receptor, devolvemos si está conectado o no y su puerto de conexión
            if (strcmp(name, receiver) == 0) {
                *recv_connected = connected;
                strcpy(recv_ip, ip);
                *recv_port = port;
            }
            
            // copiamos la linea al nuevo fichero temporal
            int len = snprintf(line_buf, sizeof(line_buf), "%s %d %s %d %u\n", name, connected, ip, port, last_id);
            write(fd_temp, line_buf, len);
        }
    }
    
    // eliminamos usuarios.txt y renombramos el temporal
    close(fd);
    close(fd_temp);
    unlink(DB_USERS_FILE);
    rename(DB_TEMP_FILE, DB_USERS_FILE);

    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion que guarda un mensaje en el fichero de un usuario
int db_store_message(const char *receiver, const char *sender, unsigned int msg_id, const char *message, const char *fileName) {
    pthread_mutex_lock(&db_mutex);
    
    // obtenemos el nombre del fichero mensajes del usuario y su ruta
    char msg_file[300];
    snprintf(msg_file, sizeof(msg_file), "database/mensajes_%s.dat", receiver);
    
    // intentamos abrir el fichero anterior o si no existe lo creamos
    int fd = open(msg_file, O_WRONLY | O_CREAT | O_APPEND, 0644); 
    if (fd < 0) {
        pthread_mutex_unlock(&db_mutex);
        return 2;
    }
    
    // rellenamos la estructura mensaje
    PendingMsg msg;
    memset(&msg, 0, sizeof(PendingMsg));
    strncpy(msg.sender,  sender,  255); 
    msg.sender[255]  = '\0';
    msg.id = msg_id;
    strncpy(msg.message, message, 255); 
    msg.message[255] = '\0';

    // Parte2: si el filename no es nulo, tambien lo añadimos a la estructura
    if (fileName != NULL) {
        strncpy(msg.fileName, fileName, 255); msg.fileName[255] = '\0';
    }

    // escribimos el struct anterior en el fichero del usuario
    write(fd, &msg, sizeof(PendingMsg));
    
    close(fd);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}

// funcion que elimina un mensaje de un archivo de un usuario
int db_delete_message(const char *userName, unsigned int msg_id) {
    pthread_mutex_lock(&db_mutex);
    
    // obtenemos el nombre del fichero mensajes del usuario y su ruta
    char msg_file[300];
    snprintf(msg_file, sizeof(msg_file), "database/mensajes_%s.dat", userName);
    
    // intentamos abrir el fichero anterior
    int fd = open(msg_file, O_RDONLY);
    if (fd < 0) {
        pthread_mutex_unlock(&db_mutex);
        return 0; 
    }
    
    // creamos un fichero temporal 
    char temp_file[300];
    snprintf(temp_file, sizeof(temp_file), "database/temp_mensajes_%s.dat", userName);
    int fd_temp = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    PendingMsg msg;
    
    // leemos linea por linea el mensaje y si esa linea no lo es, lo copiamos en el fichero temporal
    while (read(fd, &msg, sizeof(PendingMsg)) == sizeof(PendingMsg)) {
        if (msg.id != msg_id) { 
            write(fd_temp, &msg, sizeof(PendingMsg));
        }
    }
    

    // eliminamos el fichero de los mensajes de usuario y renombramos el temporal
    close(fd);
    close(fd_temp);
    
    unlink(msg_file);
    rename(temp_file, msg_file);
    
    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion que obtiene todos los mensajes pendientes de un usuario. 
int db_get_pending_messages(const char *userName, PendingMsg **msgs, int *numMsgs) {
    pthread_mutex_lock(&db_mutex);
    
    // obtenemos el nombre del fichero mensajes del usuario y su ruta
    char msg_file[300];
    snprintf(msg_file, sizeof(msg_file), "database/mensajes_%s.dat", userName);
    
    // intentamos abrir el fichero
    int fd = open(msg_file, O_RDONLY);
    if (fd < 0) {
        *msgs = NULL;
        *numMsgs = 0;
        pthread_mutex_unlock(&db_mutex);
        return 0; 
    }
    
    // nos ponemos al principio del archivo
    long size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    // vemos el numero de mensajes que tenemos que entregar
    *numMsgs = size / sizeof(PendingMsg);
    
    if (*numMsgs > 0) {
        // Reservamos la memoria RAM necesaria
        *msgs = malloc(size); 
        // Leemos todos los bytes de golpe en nuestro array bloqueante
        read(fd, *msgs, size);
    } else {
        *msgs = NULL;
    }
    
    close(fd);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion que devuelve todos los usuarios conectados ahora mismo 
int db_get_connected_users(const char *requestingUser, char ***usersList, int *numUsers) {
    pthread_mutex_lock(&db_mutex);
    
    // abrimos el fichero de usuarios
    int fd = open(DB_USERS_FILE, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&db_mutex); return 2; }

    char line_buf[512];
    char name[256], ip[20];
    int connected, port;
    unsigned int last_id;
    int req_exists = 0, req_connected = 0, count = 0;

    // leemos linea por linea para verificar que el usuario está conectado y registrado
    while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {
        if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {
            
            // si está conectado y registrado marcamos los flags 
            if (strcmp(name, requestingUser) == 0) {
                req_exists = 1;
                req_connected = connected;
            }

            // vamos contando cuantos usuarios hay conectados en count
            if (connected == 1) count++;
        }
    }

    // Si el usuario no esta conectado o registrado cerramos todo
    if (!req_exists) { close(fd); pthread_mutex_unlock(&db_mutex); return 2; }
    if (!req_connected) { close(fd); pthread_mutex_unlock(&db_mutex); return 1; }

    *numUsers = count;

    // Si no hay nadie conectado, userlist es null
    if (count == 0) {
        *usersList = NULL;
    
    } else {

        // Si hay alguien conectado, reservamos memoria para almacenar el nombre de los usuarios
        *usersList = malloc(count * sizeof(char *));
        
        // Rebobinamos el descriptor
        lseek(fd, 0, SEEK_SET);
        
        int idx = 0;
        
        // leemos de nuevo linea por linea para encontrar quien está conectado y quien no
        while (read_line_sys(fd, line_buf, sizeof(line_buf)) > 0) {
            if (sscanf(line_buf, "%255s %d %19s %d %u", name, &connected, ip, &port, &last_id) == 5) {

                // si el usuario de esta linea está conectado, lo añadimos a userlist
                // Parte 2: ahora devuelve el nuevo formato
                if (connected == 1) {
                    char buffer[400];
                    snprintf(buffer, sizeof(buffer), "%s :: %s :: %d", name, ip, port);
                    (*usersList)[idx] = strdup(buffer);
                    idx++;
                }
            }
        }
    }

    // cerramos todo
    close(fd);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}



// funcion que libera la memoria del array de usuarios devuelto por db_get_connected_users
void db_free_users_list(char **usersList, int numUsers) {
    if (usersList) {
        for (int i = 0; i < numUsers; i++) free(usersList[i]);
        free(usersList);
    }
}

