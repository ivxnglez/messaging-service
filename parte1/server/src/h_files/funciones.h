


// aqui iran las funciones register, send, unregister ...



void f_register(int sd, char* username);

void f_unregister(int sd, char* username);

void f_connect(int sd, char* username, int puerto_escucha);

void f_disconnect(int sd, char* username);

void f_users(int sd, char* username);

void f_send(int sd, char* username, char* destinatario, char* message);





