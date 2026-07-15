

#ifndef DATABASE_H
#define DATABASE_H

// Estructura para almacenar los mensajes pendientes de forma segura
typedef struct {
    char sender[256];
    unsigned int id;
    char message[256];
    char fileName[256]; // parte2: aqui irá el nombre del filename
} PendingMsg;

int db_init();

int db_register(const char *userName);

int db_unregister(const char *userName);

int db_connect(const char *userName, const char *ip, int port);

int db_disconnect(const char *userName);

int db_prepare_message(const char *sender, const char *receiver, unsigned int *msg_id, int *recv_connected, char *recv_ip, int *recv_port);

int db_store_message(const char *receiver, const char *sender, unsigned int msg_id, const char *message, const char *fileName);

int db_delete_message(const char *userName, unsigned int msg_id);

int db_get_pending_messages(const char *userName, PendingMsg **msgs, int *numMsgs);

int db_get_connected_users(const char *requestingUser, char ***usersList, int *numUsers);

void db_free_users_list(char **usersList, int numUsers);

#endif // DATABASE_H