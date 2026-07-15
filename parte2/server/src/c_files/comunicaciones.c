

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comunicaciones.h"


// Función auxiliar para enviar (de las diapositivas)
int sendMessage(int socket, char *buffer, int len)
{
    int r;
    int l = len;

    do {
        r = write(socket, buffer, l);
        l = l - r;
        buffer = buffer + r;
    } while ((l > 0) && (r >= 0));

    if (r < 0) {
        return (-1); /* fallo */
    } else {
        return (0);  /* se ha enviado longitud */
    }
}

// Función auxiliar para recibir (de las diapositivas)
int recvMessage(int socket, char *buffer, int len)
{
    int r;
    int l = len;

    do {
        r = read(socket, buffer, l);
        l = l - r;
        buffer = buffer + r;
    } while ((l > 0) && (r >= 0));

    if (r < 0) {
        return (-1); /* fallo */
    } else {
        return (0);  /* se ha recibido longitud */
    }
}

// Lee del socket byte a byte hasta encontrar un '\0'
int leer_cadena(int socket, char *buffer, int max_len) {
    char c;
    int leidos = 0;
    int n;

    while (leidos < max_len - 1) {
        n = read(socket, &c, 1);
        if (n <= 0) {
            return -1; // Error o conexión cerrada
        }
        buffer[leidos++] = c;
        if (c == '\0') {
            break; // Cadena completa
        }
    }
    buffer[leidos] = '\0'; // Asegurar el final por seguridad
    return leidos;
}

