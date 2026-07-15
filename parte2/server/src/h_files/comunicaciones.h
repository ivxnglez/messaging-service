

#ifndef COMUNICACIONES_H
#define COMUNICACIONES_H

// Funciones recicladas de las diapositivas, ya usadas en el ej2
int sendMessage(int socket, char *buffer, int len);

int recvMessage(int socket, char *buffer, int len);

int leer_cadena(int socket, char *buffer, int max_len);

#endif
