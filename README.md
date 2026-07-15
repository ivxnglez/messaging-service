## Para compilar

### Primera parte
Para compilar la Parte 1 del proyecto en su totalidad, únicamente es necesario ejecutar el siguiente comando desde la carpeta parte1/server:

Compilación del servidor: make

Esto generará el ejecutable server en el directorio parte1/server. El cliente, al estar implementado en Python, no requiere compilación. Para lanzar el sistema, en primer lugar debe arrancarse el servidor indicando el puerto en el que escuchará mediante el flag -p:

Ejecución del servidor: ./server -p <Puerto>

A continuación, en otra terminal, se ejecuta el cliente Python indicando la IP del servidor y el puerto con los flags -s y -p respectivamente:

Ejecución del cliente: python3 client.py -s <IP> -p <Puerto>


### Segunda parte 
Para compilar la Parte 2 del proyecto, empezamos con la compilación del servidor de mensajería, que se realiza con el mismo comando que en la primera parte:

Compilación del servidor de mensajería: make

El servidor de registro RPC se compila de forma independiente desde la carpeta parte2/rpc_server:

Compilación del servidor RPC: make

El servicio web de normalización no requiere compilación, pero sí es necesario instalar sus dependencias mediante pip antes de arrancarlo:

Instalación de dependencias: pip install -r requirements.txt

Para la ejecución completa del sistema en la Parte 2 es necesario arrancar los tres procesos en el orden adecuado: primero el servidor RPC de log, después el servicio web de normalización y por último el servidor de mensajería. El servidor de log se arranca simplemente ejecutando su binario, aunque si no se tiene activado el rpc_bind habrá que hacerlo con el siguiente comando: sudo systemctl start rpcbind

Una vez hecho esto ejecutamos el binario:
Servidor RPC: ./log_rpc_server

El servicio web se arranca desde la carpeta parte2/web_service:
Servicio web: python3 ws_normalize.py
Finalmente, el servidor de mensajería se lanza con la variable de entorno LOG_RPC_IP apuntando a la máquina donde corre el servidor de log (normalmente localhost en pruebas locales):

Servidor de mensajería: LOG_RPC_IP=localhost ./server -p <Puerto>

El cliente Python de la Parte 2 se ejecuta de la misma forma que en la Parte 1:

Cliente: python3 client.py -s <IP> -p <Puerto>
