/*
 * log_rpc.x - Interfaz IDL/XDR del servicio de registro de operaciones.
 *
 * Este fichero define el contrato entre el servidor de mensajería (que actúa
 * como cliente RPC) y el servidor RPC de log (que imprime las operaciones).
 *
 * rpcgen leerá este fichero y generará automáticamente:
 *   - log_rpc.h        : tipos y prototipos compartidos por cliente y servidor
 *   - log_rpc_clnt.c   : stub del cliente (lo usa el servidor de mensajería)
 *   - log_rpc_xdr.c    : serialización/deserialización de los tipos XDR
 *   - log_rpc_svc.c    : dispatcher del servidor RPC (lo usa rpc_server/)
 */

/* Alias para cadena C de hasta 256 caracteres.
 * Se usa para los tres argumentos: usuario, operacion y fichero. */
typedef string cadena<256>;

/* Definición del programa RPC con su número único y su versión. */
program LOG_RPC {
    version LOG_RPC_VERSION {

        /*
         * Hemos hecho una uńica operacion, ya que realmente solo tiene hacer una cosa 
         * imprimir por pantalla cada vez que un usuario haga algo
         *
         * Parámetros:
         *   usuario   : nombre del usuario que realiza la operación.
         *   operacion : nombre de la operación (REGISTER, UNREGISTER,
         *               CONNECT, DISCONNECT, USERS, SEND, SENDATTACH).
         *   fichero   : nombre del fichero adjunto. Solo relevante en
         *               SENDATTACH; en el resto se pasa cadena vacía "".
         *
         * Devuelve: 0 si OK, -1 si error.
         */
        int LOG_OPERATION(cadena usuario, cadena operacion, cadena fichero) = 1;

    } = 1;  

} = 0x0522281;
