#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include "jsocket6.4.h"
#include "Data.h"
#include <pthread.h>
#include <sys/poll.h>

#define BUFFER_LENGTH 1400 //hay que definir que largo vamos a usarrrr

struct timeval t0, t1, t2;
struct timeval timeout;
fd_set set;
int fd[2]; 
struct pollfd readstatus;
readstatus.fd = bwss;
readstatus.events = POLLIN;
int portTCP;
char *portUDP="2001";
int sudp;
int stcp;
int rdy=0;
int serie=0;
void* funcionTCP(void *puerto);



//funcion auxiliar que hace Dbind
//creada para no enrtegar inputs en el thread
void *DbindAux(){
	Dbind(funcionTCP,"2001");
	return NULL;
}


//no estoy segura si el buffer se pasa asi!
void agregarHeader(int[] buff,char letra, int numSerie,int largo){
	char buffer1[BUFFER_LENGTH];
	buffer1=buffer;
	//ponemos el header en buffer1. 
	buffer[0]=letra;
	char str[5];
	to_char_seq(numSerie, str) //dejamos numSerie en str

	//nose si este for esta bien en el rango!
	for(int i = 1; i <6; i++){
		buffer[i]=str[i-1]; //en el header chanto los elementos del string 
	}

	//for para meter todo lo nuevo
	for(int i = 6; i<largo; i++){
		buffer[i]=buffer1[i-6];
	}
}

//metodo que espera el ack a traves del pipe
//revisa que el num de serie sea igual, si es != retorna 0
//si se acaba el tiempo retorna 0  
int esperarACK(int numSerie){

	//tiempo que espera la respuesta
	timeout.tv_sec=1;
	timeout.tv_usec=0;
	int rc;
	char res;
	//recibe del pipe
	rc = poll(&readstatus, 1, 1000);
	//si se acabo el tiempo
	if(rc==0){
		return 0;
	}
	else if (rc < 0)
    {
      perror("  poll() failed");
      break;
    }
	//si el ack q recibo es el q estoy esperando
	else if (rc >= 1 && read(fd[0], (int)res, sizeof(int)) == numSerie){
		return 1;
	}
	//si el numero es diferente
	else {
		return 0;
	}	
}

/*recibe tcp y manda udp*/
void *funcionTCP(void *puerto){
	int bytes,cnt,packs;
	char buffer[BUFFER_LENGTH];
	gettimeofday(&t0,NULL);


	portTCP=*((int*)puerto);
	free(puerto);
	fprintf(stderr, "cliente tcp conectado\n");

	for(bytes=0,packs=0;; bytes+=cnt,packs++) {
		cnt = Dread(portTCP, buffer, BUFFER_LENGTH);
		if(bytes == 0)
			gettimeofday(&t1, NULL); /* Mejor esperar al primer read para empezar a contar */
			write(sudp,NULL,0); //primer mensaje a udp
		if(cnt <= 0) 
			break;
		//hay que agregar el header para mandar
		cnt+=6;
		//se le agrega header al buffer y de manda
		//NO ESTOY SEGURA SI LOS BUFFER ESTAN FUNCIONANDO BIEN!
		buffer = agregarHeader(buffer,"D",serie,cnt) 
		//intentar enviar hasta que reciba el ack 
		while(esperarACK(serie) == 0)
			write(sudp, buffer, cnt); //devuelve 0 pq llego el ack y reenvio 
    }	

	return NULL;
}

//funcion que lee por udp y manda por TCP las cosas de vuelta
//si se lee un ACK entonces 
void *funcionUDP(){
	int bytes, cnt;
	char buffer[BUFFER_LENGTH];

	for(bytes=0;;bytes+=cnt) {
		if((cnt=read(sudp, buffer, BUFFER_LENGTH)) <= 0){ //ya no queda nada mas para leer
		    break;
	        
	    }

	    Dwrite(portTCP, buffer, cnt); //devolvemos cosas a stcp
	    
	}
	//Dwrite(stcp, buffer, 0); //avisa que termino
	rdy=1;
	Dclose(stcp);
    close(sudp);
	return NULL;
}


int main(){
	sudp = j_socket_udp_connect("localhost","2000");
    if(sudp < 0) {
	printf("connect UDP failed\n");
       	exit(1);
    }

    printf(" UDP conectado\n");
    pthread_t pid1;
    pthread_t pid2;

	pthread_create(&pid1,NULL,DbindAux,NULL);
	pthread_create(&pid2,NULL,funcionUDP,NULL);

	//conexion al pipe
	int result = pipe(fd);
	if(result<0){
		perror("pipe ");
		exit(1);
	}

	while(!rdy){
		;
	}

	pthread_join(pid1,NULL);
	pthread_join(pid2,NULL);
	
	return 0;

}
