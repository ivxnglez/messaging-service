

/* Alias para cadena C de hasta 256 caracteres.
 * Se usa para los tres argumentos: usuario, operacion y fichero. */
typedef string cadena<256>;

program LOG_RPC {
    version LOG_RPC_VERSION {

        /*
         * Hemos hecho una uńica operacion, ya que realmente solo tiene hacer una cosa 
         * imprimir por pantalla cada vez que un usuario haga algo
         *
         * Devuelve: 0 si OK, -1 si error.
         */
        int LOG_OPERATION(cadena usuario, cadena operacion, cadena fichero) = 1;

    } = 1;  

} = 0x0522281;