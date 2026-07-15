from enum import Enum
import argparse
import socket
import threading
import sys

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1

    _listen_socket = None       # socket de escucha del cliente
    _listen_thread = None       # hilo encargado de escuchar
    _listen_port = -1           # puerto de escucha
    _connected_user = None      # nombre de usuario actualmente conectado 
    _stop_listening = False     # flag para indicar al hilo que pare de escuchar


    # ******************** METHODS *******************

    # método que se encarga de connectarse al servidor
    @staticmethod
    def connect_to_server(ip_server, port):

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) # creamos el socket
            server_adrres = (ip_server, port) # cogemos la direccion del servidor
            sock.connect(server_adrres) # nos conectamos al servidor
            # devolvemos la estructura socket
            return sock

        except socket.error as e:
            return None



    # método que se encarga de enviar al servidor el buffer
    @staticmethod
    def send_to_server(sock: socket, mensaje: str):

        try:
            sock.sendall(mensaje.encode())
            return True

        except socket.error as e:
            return False



    # Método para recibir exactamente 1 byte numérico del servidor
    @staticmethod
    def receive_byte(sock: socket.socket):
        try:
            byte_recibido = sock.recv(1) # recibimos un byte del servidor
            if not byte_recibido:
                return -1 # El servidor cerró la conexión
            # Convertimos el byte crudo a un entero (0, 1, 2...)
            return int.from_bytes(byte_recibido, byteorder='big')

        except socket.error as e:
            return -1


    # Método auxiliar para recibir una cadena terminada en \0
    @staticmethod
    def receive_string(sock: socket.socket):
        cadena = ""
        try:
            while True:
                # Leemos de 1 en 1 byte
                char = sock.recv(1)

                # Si el servidor corta la conexión o llega el \0, paramos
                if not char or char == b'\0':
                    break

                # Decodificamos el byte a texto y lo añadimos
                cadena += char.decode()

            return cadena
        except socket.error:
            return None



    # método que ejecuta el hilo de escucha de mensajes entrantes
    @staticmethod
    def listener_thread():
        while not client._stop_listening:
            try:
                # ponemos un timeout corto para poder mirar la flag de parada periódicamente
                client._listen_socket.settimeout(1.0)
                try:
                    conn, addr = client._listen_socket.accept()
                except socket.timeout:
                    continue
                except OSError:
                    # el socket fue cerrado desde fuera (disconnect)
                    break

                # ya tenemos una conexión del servidor, leemos la respuesta
                operacion = client.receive_string(conn)
                if operacion is None:
                    conn.close()
                    continue

                # si llega un mensaje desde el servidor de otro usuario, lo imprimimos por terminal
                if operacion == "SEND_MESSAGE":
                    # cogemos los datos
                    remitente = client.receive_string(conn)
                    id_msg = client.receive_string(conn)
                    mensaje = client.receive_string(conn)

                    # comprobamos que no haya errores en el mensaje recibido y los impriminos
                    if remitente is not None and id_msg is not None and mensaje is not None:
                        print(f"MESSAGE {id_msg} FROM {remitente}")
                        print(mensaje)
                        print("END")

                # si llega un mensaje desde el servidor confirmando que se envió, lo imprimimos por terminal
                elif operacion == "SEND MESS_ACK":

                    #cogemos los datos y los imprimimos
                    id_msg = client.receive_string(conn)
                    if id_msg is not None:
                        print(f"SEND MESSAGE {id_msg} OK")

                conn.close()

            except Exception:
                # cualquier excepción no controlada en el hilo no debe matar el cliente
                if client._stop_listening:
                    break
                continue



    # método auxiliar que para el hilo de escucha y libera recursos
    @staticmethod
    def stop_listener():
        client._stop_listening = True

        # cerramos el socket de escucha
        if client._listen_socket is not None:
            try:
                client._listen_socket.close()
            except Exception:
                pass
            client._listen_socket = None

        # cerramos el hilo de escucha
        if client._listen_thread is not None:
            try:
                client._listen_thread.join(timeout=2.0)
            except Exception:
                pass
            client._listen_thread = None
        
        # "quitamos" el usuario conectado y el puerto
        client._listen_port = -1
        client._connected_user = None



    # *
    # * @param user - User name to register in the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        # comprobamos longitud (máx 255 caracteres + '\0')
        if len(user) > 255:
            print("REGISTER FAIL")
            return client.RC.ERROR

        # nos conectamos al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            print("REGISTER FAIL")
            return client.RC.ERROR

        # mandamos la operacion
        if not client.send_to_server(sock, "REGISTER\0"):
            print("REGISTER FAIL")
            sock.close()
            return client.RC.ERROR

        # mandamos el nombre de usuario
        mensaje = user + '\0'
        if not client.send_to_server(sock, mensaje):
            print("REGISTER FAIL")
            sock.close()
            return client.RC.ERROR

        # recibimos la respuesta
        respuesta = client.receive_byte(sock)

        # imprimimos el resultado exacto
        if respuesta == 0:
            print("REGISTER OK")
            resultado_final = client.RC.OK
        elif respuesta == 1:
            print("USERNAME IN USE")
            resultado_final = client.RC.USER_ERROR
        else:
            print("REGISTER FAIL")
            resultado_final = client.RC.ERROR

        # cerramos el socket
        sock.close()

        return resultado_final



    # *
    # 	 * @param user - User name to unregister from the system
    # 	 *
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
        # comprobamos longitud
        if len(user) > 255:
            print("UNREGISTER FAIL")
            return client.RC.ERROR

        # nos conectamos al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            print("UNREGISTER FAIL")
            return client.RC.ERROR

        # mandamos la operacion
        if not client.send_to_server(sock, "UNREGISTER\0"):
            print("UNREGISTER FAIL")
            sock.close()
            return client.RC.ERROR

        # mandamos el nombre de usuario
        mensaje = user + '\0'
        if not client.send_to_server(sock, mensaje):
            print("UNREGISTER FAIL")
            sock.close()
            return client.RC.ERROR

        # recibimos la respuesta
        respuesta = client.receive_byte(sock)

        # imprimimos el resultado exacto (según enunciado, sección 6.3: "USER DOES NOT EXIST")
        if respuesta == 0:
            print("UNREGISTER OK")
            resultado_final = client.RC.OK
        elif respuesta == 1:
            print("USER DOES NOT EXIST")
            resultado_final = client.RC.USER_ERROR
        else:
            print("UNREGISTER FAIL")
            resultado_final = client.RC.ERROR

        # cerramos el socket
        sock.close()

        return resultado_final


    # *
    # * @param user - User name to connect to the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        # comprobamos longitud
        if len(user) > 255:
            print("CONNECT FAIL")
            return client.RC.ERROR

        # comprobamos que no haya ya un usuario conectado en este cliente
        if client._connected_user is not None:
            print("USER ALREADY CONNECTED")
            return client.RC.USER_ERROR

        # creamos el socket de escucha
        try:
            listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listen_sock.bind(('0.0.0.0', 0)) # de esta forma el sistema operativo asigna un puerto automaticamente
            listen_sock.listen(10)
            puerto_escucha = listen_sock.getsockname()[1]
        except socket.error:
            print("CONNECT FAIL")
            return client.RC.ERROR

        # creamos el hilo de escucha 
        client._listen_socket = listen_sock
        client._listen_port = puerto_escucha
        client._stop_listening = False
        client._listen_thread = threading.Thread(target=client.listener_thread, daemon=True)
        client._listen_thread.start()

        # enviamos la solicitud de conexión al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL")
            return client.RC.ERROR

        # mandamos la operación
        if not client.send_to_server(sock, "CONNECT\0"):
            sock.close()
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL")
            return client.RC.ERROR

        # mandamos el nombre de usuario
        if not client.send_to_server(sock, user + '\0'):
            sock.close()
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL")
            return client.RC.ERROR

        # mandamos el puerto como cadena
        if not client.send_to_server(sock, str(puerto_escucha) + '\0'):
            sock.close()
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL")
            return client.RC.ERROR

        # recibimos respuesta
        respuesta = client.receive_byte(sock)
        sock.close()

        if respuesta == 0:
            # dejamos el hilo escuchando y guardamos el usuario conectado
            client._connected_user = user
            print("CONNECT OK")
            return client.RC.OK
        
        elif respuesta == 1:
            # si no existe el usuario, paramos el hilo
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        elif respuesta == 2:
            # si ya está conectado paramos el hilo
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("USER ALREADY CONNECTED")
            return client.RC.USER_ERROR
        
        else:
            # si obtenemos un 3, hay un error en el servidor
            client._stop_listening = True
            try:
                listen_sock.close()
            except Exception:
                pass
            client._listen_socket = None
            print("CONNECT FAIL")
            return client.RC.ERROR


    # *
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :

        # vemos si el usuario se ha conectado antes de hacer nada
        if client._connected_user is None:
            print("CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return client.RC.USER_ERROR

        # nos conectamos al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            print("CONNECTED USERS FAIL")
            return client.RC.ERROR

        # mandamos la operación
        if not client.send_to_server(sock, "USERS\0"):
            sock.close()
            print("CONNECTED USERS FAIL")
            return client.RC.ERROR

        # mandamos el nombre del usuario que solicita
        if not client.send_to_server(sock, client._connected_user + '\0'):
            sock.close()
            print("CONNECTED USERS FAIL")
            return client.RC.ERROR

        # recibimos respuesta
        respuesta = client.receive_byte(sock)

        # si ha habido exito, imprimimos los nombres
        if respuesta == 0:
            # si no recibimos ninguna cadena ha habido un error
            cadena_n = client.receive_string(sock)
            if cadena_n is None:
                sock.close()
                print("CONNECTED USERS FAIL")
                return client.RC.ERROR
            try:
                # obtenemos el numero de usuarios
                n = int(cadena_n)
            except ValueError:
                sock.close()
                print("CONNECTED USERS FAIL")
                return client.RC.ERROR

            # imprimimos la cabecera 
            print(f"CONNECTED USERS ({n} users connected) OK")

            # leemos todos los nombres de los usuarios registrados
            for _ in range(n):
                nombre = client.receive_string(sock)
                if nombre is None:
                    sock.close()
                    print("CONNECTED USERS FAIL")
                    return client.RC.ERROR
                print(nombre)

            sock.close()
            return client.RC.OK

        # si por alguna razon el principio de la funcion no capta que el usuario no está cocnectado, mostramos el error
        elif respuesta == 1:
            sock.close()
            print("CONNECTED USERS FAIL, USER IS NOT CONNECTED")
            return client.RC.USER_ERROR
        
        # ha habido otro error
        else:
            sock.close()
            print("CONNECTED USERS FAIL")
            return client.RC.ERROR



    # *
    # * @param user - User name to disconnect from the system
    # *
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        # comprobamos longitud
        if len(user) > 255:
            print("DISCONNECT FAIL")
            return client.RC.ERROR

        # nos conectamos al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            client.stop_listener()
            print("DISCONNECT FAIL")
            return client.RC.ERROR

        # mandamos operación
        if not client.send_to_server(sock, "DISCONNECT\0"):
            sock.close()
            client.stop_listener()
            print("DISCONNECT FAIL")
            return client.RC.ERROR

        # mandamos el nombre de usuario
        if not client.send_to_server(sock, user + '\0'):
            sock.close()
            client.stop_listener()
            print("DISCONNECT FAIL")
            return client.RC.ERROR

        # recibimos respuesta (0, 1, 2 ó 3)
        respuesta = client.receive_byte(sock)
        sock.close()

        # paramos sí o sí el hilo de escucha
        client.stop_listener()

        # imprimos en la terminal segun la respuesta
        if respuesta == 0:
            print("DISCONNECT OK")
            return client.RC.OK
        
        elif respuesta == 1:
            print("DISCONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        elif respuesta == 2:
            print("DISCONNECT FAIL, USER NOT CONNECTED")
            return client.RC.USER_ERROR
        
        else:
            print("DISCONNECT FAIL")
            return client.RC.ERROR


    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        # si no hay ningun usuario conectado, mostramos un error
        if client._connected_user is None:
            print("SEND FAIL")
            return client.RC.ERROR

        # comprobamos longitud del nombre destinatario
        if len(user) > 255:
            print("SEND FAIL")
            return client.RC.ERROR

        # comprobamos longitud del mensaje
        if len(message) > 255:
            print("SEND FAIL")
            return client.RC.ERROR

        # nos conectamos al servidor
        sock = client.connect_to_server(client._server, client._port)
        if sock is None:
            print("SEND FAIL")
            return client.RC.ERROR

        # mandamos la operación
        if not client.send_to_server(sock, "SEND\0"):
            sock.close()
            print("SEND FAIL")
            return client.RC.ERROR

        # mandamos el nombre del remitente
        if not client.send_to_server(sock, client._connected_user + '\0'):
            sock.close()
            print("SEND FAIL")
            return client.RC.ERROR

        # mandamos el nombre del destinatario
        if not client.send_to_server(sock, user + '\0'):
            sock.close()
            print("SEND FAIL")
            return client.RC.ERROR

        # mandamos el mensaje
        if not client.send_to_server(sock, message + '\0'):
            sock.close()
            print("SEND FAIL")
            return client.RC.ERROR

        # recibimos la respuesta
        respuesta = client.receive_byte(sock)

        # si nos devuelve un 0, se ha envidado todo bien
        if respuesta == 0:
            id_msg = client.receive_string(sock)
            sock.close()
            
            # si no nos manda un id, ha ocurrido un error en el proceso
            if id_msg is None:
                print("SEND FAIL")
                return client.RC.ERROR
            
            # imprimimos en la terminal el id del mensaje
            print(f"SEND OK - MESSAGE {id_msg}")
            return client.RC.OK
        
        # si nos devuelve un 1, el destinatario no existe
        elif respuesta == 1:
            sock.close()
            print("SEND FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        # si nos devuelve un 2, ha habido un error
        else:
            sock.close()
            print("SEND FAIL")
            return client.RC.ERROR


    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # *
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user,  file,  message) :
        return client.RC.ERROR

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            #  Remove first two words
                            message = ' '.join(line[3:])
                            client.sendAttach(line[1], line[2], message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # antes de salir, paramos el hilo de escucha si estuviera activo
                            client.stop_listener()
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;

        # asignamos a los atributos de clase (importante: usar client._server, no _server)
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")


if __name__=="__main__":
    client.main([])