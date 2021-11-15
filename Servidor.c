#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <mysql.h>
#include <pthread.h>
#include <ctype.h>

typedef struct{
	char usuario[20];
	int socket;
} Conectado;

typedef struct{
	Conectado sockets[300];
	int num;
} ListaConectados;

typedef struct{
	int ID; 
	char fecha[20];
	char hora_final[20];
	int duracion;
	char ganador[20];
}Partida;

typedef struct{
	int num;
	Partida partidas[300];
}ListaPartidas;

ListaConectados UsuariosConectados;
ListaConectados UsuariosPartida;
ListaPartidas listadePartidas;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int AddUsuarioConectado (ListaConectados *lista, int socket, char nombre[20]) //Añadir un usuario a la lista de conectados
{
	
	if (lista->num == 300)
		return -1;
	else {
		lista->sockets[lista->num].socket=socket;
		strcpy(lista->sockets[lista->num].usuario,nombre);
		lista->num++;
		return 0;
		
	}
}

int Add (ListaConectados *lista, int socket, char nombre[20]) //AÃ±adir un usuario a la lista de conectados
{
	
	if (lista->num == 300)
		return -1;
	else {
		lista->sockets[lista->num].socket=socket;
		strcpy(lista->sockets[lista->num].usuario,nombre);
		lista->num++;
		return 0;
		
	}
}

void DameConectados (ListaConectados *lista, char conectados[512]) //Devuelve los usuarios conectados
{
	sprintf(conectados,"7/%d/%s",lista->num, lista->sockets[0].usuario);
	int i;
	for(int i=1; i< lista->num; i++)
		sprintf(conectados,"%s/%s", conectados, lista->sockets[i].usuario);
	
	printf("Conectados: %s\n",conectados);
}

int DesconectarUsuario(ListaConectados *lista, char nombre[20]) //Elimina un usuario de la lista
{
	int j=0;
	int encontrado=0;
	while((j<lista->num) && !encontrado){
		if (strcmp(lista->sockets[j].usuario,nombre)==0)
			encontrado=1;
		if(!encontrado)
			j=j+1;
	}
	if (encontrado)
	{
		for(int pos=j; pos<lista->num-1; pos++)
		{
			strcpy(lista->sockets[pos].usuario,lista->sockets[pos+1].usuario);
			lista->sockets[pos].socket=lista->sockets[pos+1].socket;
		}
		lista->num--;
		return 0;
	}
	
	else
		return -1;
	
}

int DameNombre(ListaConectados *l, int socket, char nombre[20]) //Devuelve el nombre del usuario con ese socket	
{
	int found = 0;
	int i = 0;
	while ((found==0) & (i < l->num))
	{
		if (l->sockets[i].socket==socket)
		{
			found = 1;
		}
		else
			i = i + 1;
	}
	if (found==1)
	{
		strcpy(nombre,l->sockets[i].usuario);
		return 1;
	}
	else
		return -1;
}

void *AtenderalCliente (void *socket) //Funcion para atender al cliente
{
	int sock_conn;
	int *s;
	s= (int *) socket;
	sock_conn= *s;
	
	MYSQL *conn;
	int err;
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	char consulta [500];
	conn = mysql_init(NULL);
	
	char peticion[512];
	char respuesta[512];
	int ret;
	
	if (conn==NULL) 
	{
		printf ("Error al crear la conexion: %u %s\n",mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	//Conectamos con la base de datos
	conn = mysql_real_connect (conn, "shiva2.upc.es","root", "mysql", "M2_BBDDJuego",0, NULL, 0);
	if (conn==NULL) 
	{
		printf ("Error al inicializar la conexion: %u %s\n",mysql_errno(conn), mysql_error(conn));
		exit (1);
	}
	
	//Bucle para atender al cliente
	char notificacion[500];
	int terminar=0;
	while (terminar==0)
	{
		ret=read(sock_conn,peticion, sizeof(peticion));
		if (ret == 0)
		{
			terminar = 1;
			continue;
		}
		printf ("Recibido\n");
		
		peticion[ret]='\0';
		
		printf ("Mensaje recibido: %s\n",peticion);
		
		char *p = strtok( peticion, "/"); //Sacamos el codigo de la peticion del cliente
		int codigo =  atoi (p);
		printf("codigo= %d\n",codigo);
		
		if (codigo==0) //Desconectar Usuario
		{ 
			char usuario[20];
			DameNombre(&UsuariosConectados,sock_conn,usuario);			
			
			pthread_mutex_lock( &mutex );
			int eliminar = DesconectarUsuario(&UsuariosConectados, usuario);
			int eliminar_partida = DesconectarUsuario(&UsuariosPartida, usuario);
			pthread_mutex_unlock( &mutex );
			char conectados[300];
			DameConectados(&UsuariosConectados, conectados); 
			sprintf(notificacion,"%s", conectados);
			for(int k=0; k<UsuariosConectados.num; k++)
				write (UsuariosConectados.sockets[k].socket, notificacion, strlen(notificacion));
			if (eliminar==0)
			{
				printf("El usuario se ha eliminado de la lista de conectados\n");
				strcpy(respuesta,"0/");		
				write(sock_conn, respuesta, strlen(respuesta));
			}
			else
				printf("El usuario no se ha podido eliminar de la lista de conectados\n");
		}
		
		else if(codigo==1) //Iniciar sesion
		{
			char usuario[20];
			char password[20];
			p=strtok(NULL, "/");
			strcpy(usuario, p);
			p=strtok(NULL, "/");
			strcpy(password, p);
			
			err=mysql_query (conn, "SELECT * FROM Jugador");
			if (err!=0) 
			{
				printf ("Error al consultar datos de la base %u %s\n",mysql_errno(conn), mysql_error(conn));
				exit (1);
			}
			resultado = mysql_store_result (conn);
			row = mysql_fetch_row (resultado);
			if (row == NULL)
				printf ("No se han obtenido datos en la consulta\n");
			else
			{
				while (row != NULL) 
				{
					if (strcmp (row[1],usuario) ==0)
					{
						if(strcmp(row[2],password)==0)
						{
							strcpy(respuesta,"1/1");
							
							pthread_mutex_lock( &mutex );
							int res= Add(&UsuariosConectados,sock_conn,usuario);
							pthread_mutex_unlock(&mutex);
							sprintf(notificacion,"7/%d/%s",UsuariosConectados.num,UsuariosConectados.sockets[0].usuario);
							for(int j=1; j< UsuariosConectados.num; j++)
								sprintf(notificacion,"%s/%s", notificacion, UsuariosConectados.sockets[j].usuario);
							for(int k=0; k<UsuariosConectados.num; k++)
								write (UsuariosConectados.sockets[k].socket, notificacion, strlen(notificacion));
							if (res==0)
							{
								printf("Notificacion: %s\n",notificacion);
								printf("El usuario %s se ha aÃ±adido a la lista de conectados \n", usuario );
							}
							else
								printf("La lista de usuarios esta llena");
							
						}
						else 
						{
							strcpy(respuesta,"1/0");
						}
						row = NULL;
					}
					else 
					{
						row = mysql_fetch_row (resultado);
						strcpy(respuesta,"1/0");
					}	
				}
			}
			write(sock_conn, respuesta, strlen(respuesta));

		}
		else if (codigo==2) //Registrar
		{
			char usuario[20];
			char password[20];
			int ultimo_ID;
			p=strtok(NULL, "/");
			strcpy(usuario, p);
			p=strtok(NULL, "/");
			strcpy(password, p);
			
			
			err=mysql_query (conn, "SELECT * FROM Jugador");
			if (err!=0) 
			{
				printf ("Error al consultar datos de la base %u %s\n",mysql_errno(conn), mysql_error(conn));
				exit (1);
			}
			resultado = mysql_store_result (conn);
			row = mysql_fetch_row (resultado);
			if (row == NULL)
				printf ("No se han obtenido datos en la consulta\n");
			else
			{
				int encontrado=0;
				while ((row !=NULL)&&(!encontrado)) 
				{
					char name[strlen(row[1])];
					strcpy(name,row[1]);
					strcpy(respuesta, "2/1");
					ultimo_ID = atoi(row[0]);
					
					for (int k=0;k<strlen(name);k++)
					{
						name[k] = toupper(name[k]);
					}
					char user_mayu[20];
					for (int k=0;k<strlen(usuario);k++)
					{
						user_mayu[k] = toupper(usuario[k]);
					}
					user_mayu[strlen(usuario)] = '\0';
					if (strcmp (name,user_mayu) ==0)
					{
						strcpy(respuesta,"2/0");
						encontrado=1;
					}
					else row = mysql_fetch_row (resultado);
				}
				if (strcmp(respuesta,"2/1")==0)
				{
					char id[10];
					strcpy(consulta, "INSERT INTO Jugador VALUES('");
					sprintf(id,"%d", ultimo_ID +1);
					strcat(consulta, id);
					strcat (consulta, "','");
					strcat(consulta, usuario);
					strcat (consulta, "','");
					strcat(consulta, password);
					strcat(consulta, "');");
				err=mysql_query(conn, consulta);
				if (err!=0)
					{
					printf ("Error al introducir datos de la base %u %s \n", mysql_errno(conn), mysql_error(conn));
					exit(1);
					}
				}
					
				
				write(sock_conn, respuesta, strlen(respuesta));
			}
			
			
		}
		
		else if (codigo==3) //Consulta1: Dame el ganador de la partida 2
		{	
			char consulta[80];
			strcpy(consulta,"SELECT Partida.ganador FROM (Partida) WHERE Partida.id = 2");
			err=mysql_query (conn, consulta);
			if (err!=0)
			{
				printf("Error al realizar la consulta %u %s \n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado=mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			if (row == NULL)
				strcpy(respuesta,"3/Base de datos vacia");
			else
			{
				strcpy(respuesta,"3");
				while (row!=NULL){
					printf ("%s\n", row[0]);
					strcat (respuesta,"/");
					strcat (respuesta, row[0]);
					row = mysql_fetch_row (resultado);
				}
			}
			write(sock_conn, respuesta, strlen(respuesta));
		
		}
		
		else if (codigo==4) //Consulta2: Dame las partidas ganadas por Berta
		{	
			char consulta[80];
			strcpy(consulta,"SELECT Partida.id FROM Partida WHERE Partida.Ganador = 'Berta' ");
			err=mysql_query (conn, consulta);
			if (err!=0)
			{
				printf("Error al realizar la consulta %u %s \n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado=mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			if (row == NULL)
				strcpy(respuesta,"4/Base de datos vacia");
			else
			{
				strcpy(respuesta,"4");
				while (row!=NULL){
					printf ("%s\n", row[0]);
					strcat (respuesta,"/");
					strcat (respuesta, row[0]);
					row = mysql_fetch_row (resultado);
				}
			}
			write(sock_conn, respuesta, strlen(respuesta));
			
		}
		else if (codigo==5)//Consulta puntos máx. en una partida
		{
			char consulta[80];
			strcpy(consulta,"SELECT max(Resultado.puntos) FROM Resultado");
			err=mysql_query (conn, consulta);
			if (err!=0)
			{
				printf("Error al realizar la consulta %u %s \n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado=mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			if (row == NULL)
				strcpy(respuesta,"5/Base de datos vacia");
			else
			{
				strcpy(respuesta,"5");
				while (row!=NULL){
					printf ("%s\n", row[0]);
					strcat (respuesta,"/");
					strcat (respuesta, row[0]);
					row = mysql_fetch_row (resultado);
				}
			}
			write(sock_conn, respuesta, strlen(respuesta));
		}
		
		
		
	}
	close(sock_conn);
	mysql_close (conn);
	
}

int main(int argc, char *argv[])
{	
	UsuariosConectados.num=0;
	UsuariosPartida.num=0;
	listadePartidas.num=0;
	
	int sock_conn, sock_listen;
	struct sockaddr_in serv_adr;
	
	if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		printf("Error creant socket");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	serv_adr.sin_port = htons(50004);
	if (bind(sock_listen, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0)
		printf ("Error al bind\n");
	
	if (listen(sock_listen, 2) < 0)
		printf("Error en el Listen");
	
	
	pthread_t thread;
	for(;;)
	{
		printf ("Escuchando\n");
		sock_conn = accept(sock_listen, NULL, NULL);
		printf ("He recibido conexion \n");
		
		pthread_create(&thread, NULL, AtenderalCliente,&sock_conn);
		
	}
}
	
