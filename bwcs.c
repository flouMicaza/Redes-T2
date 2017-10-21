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
#include <poll.h>

#define BUFFER_LENGTH 1400 //hay que definir que largo vamos a usarrrr

struct timeval t0, t1, t2;
struct timeval timeout;
fd_set set;
int fd[2];
int portTCP;
char *portUDP="2001";
int sudp;
int stcp;
int rdy=0;
int serie=0;




void* funcionTCP(void *puerto);
int to_int_seq(unsigned char *buf);
void to_char_seq(int seq, unsigned char *buf);

//funcion auxiliar que hace Dbind
//creada para no enrtegar inputs en el thread
void *DbindAux(){
	Dbind(funcionTCP,"2001");
	return NULL;
}


//no estoy segura si el buffer se pasa asi!
//revise en la funcion Dread y lo modifique (funcion Dread esta en Data-tcp.c)
void agregarHeader(char *buff,char letra, int numSerie,char *buffer_salida){
	//ponemos el header en buffer1.
	buffer_salida[0]=letra;
	unsigned char str[5];
	to_char_seq(numSerie, str); //dejamos numSerie en str

	//nose si este for esta bien en el rango!
	for(int i = 1; i <6; i++){
		buffer_salida[i]=str[i-1]; //en el header chanto los elementos del string
	}

	//for para meter todo lo nuevo
	//ojo que hay que iterar desde 0 hasta el largo del buffer(?)
	for(int i = 6; i<BUFFER_LENGTH; i++){
		buffer_salida[i]=buff[i-6];
	}

}

//metodo que espera el ack a traves del pipe
//revisa que el num de serie sea igual, si es != retorna 0
//si se acaba el tiempo retorna 0
int esperarACK(int numSerie){
	fd_set fds;
   	struct timeval timeout;
   	int rc, res;


   	FD_ZERO(&fds);
   	FD_SET(fd[0], &fds);
   	rc = select(sizeof(fds)*4, &fds, NULL, NULL, &timeout);
   	if (rc==-1) {
    	perror("select failed");
      	return -1;
  	}

	else if (rc > 0){
		res= read(fd[0], &res, sizeof(int));
		if (res<0){ //error al leer
			perror("leer el ack salio mal");
			exit(1);
		}

		else{ //veo el valor del numero de secuencia que viene
			if(res==numSerie){
				return 1;
			}
			else {
				return 0;
			}
		}
	}
	return 0;
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
		if(bytes == 0){
			gettimeofday(&t1, NULL); /* Mejor esperar al primer read para empezar a contar */
			write(sudp,NULL,0); //primer mensaje a udp
		}

		if(cnt <= 0)
			break;

		//se le agrega header al buffer y de manda
		//NO ESTOY SEGURA SI LOS BUFFER ESTAN FUNCIONANDO BIEN!

		//cree un buffer de salida, y lo muté usando la funcion q hiciste
		char buffer_salida[BUFFER_LENGTH+DHDR];
		agregarHeader(buffer,'D',serie,buffer_salida);
		//no se si sea necesario entregarle cnt a agregarHeader

		//intentar enviar hasta que reciba el ack
		write(sudp, buffer_salida, cnt); //hay que enviarlo por primera vez(?)
		while(esperarACK(serie) == 0)
			write(sudp, buffer_salida, cnt); //devuelve 0 pq llego el ack y reenvio
		serie++;
    }

	return NULL;
}

//saca el tipo del header y el numero de secuencia
//NO ESTOY SEGURA SI SE PONE ASI LA FIRMA!!!
//cambie la firma para que nos avise que letra es (0 es A 1 es D)
int sacarHeader(char buffer[], char* letra, int*numSec,char numString[]){
	*letra = buffer[0];
	unsigned char numeros[DHDR-1];
	for (int i = 1; i < DHDR; i++){
		numString[i-1]=buffer[i];
	}
	//numString=numeros;
	int num=to_int_seq(numeros);
	*numSec=num;

	return *letra == 'D';
}

//funcion que lee por udp y manda por TCP las cosas de vuelta
//si se lee un ACK entonces se manda por el pipe
void *funcionUDP(){
	int bytes, cnt;
	char buffer[BUFFER_LENGTH+DHDR]; //(habrá que agregarle 6 bytes al buffer (?))
	char ackNumSec[DHDR]; //donde guardo el numero como string
	char ackNumSec1[DHDR]; //ack que mando con la A al ppio
	char ack;
	int numSec;

	for(bytes=0;;bytes+=cnt) {
		cnt=read(sudp, buffer, BUFFER_LENGTH);

	    //se saca el ack y el numero de secuencia que viene y se guardan en ack y numSec
	    sacarHeader(buffer,&ack,&numSec,ackNumSec); //en conf tengo si es A o D
		
		if(ack == 'D' && numSec <= serie){ //significa que no le llego mi ack
 			//mando un ack denuevo
 			ackNumSec1[0]='A';
 			//le pongo los chars del numero al ack
 			for (int i = 1; i < DHDR; ++i){

 				ackNumSec1[i]=ackNumSec[i-1];
 			}

 			write(sudp,ackNumSec1,sizeof(ackNumSec1));

 			if(cnt <= 0){ //ya no queda nada mas para leer
		    	break;
	    	}

 			else if(numSec == serie){
 				Dwrite(portTCP, buffer, cnt);
 				serie++;
 			}

 		}

 		//si lo q recibi es una A entonces es un ack que mando a traves del pipe
		else if(ack == 'A') {
			int envio= write(fd[1],&numSec,sizeof(int));

			if(envio!=1){
				perror("escribir Ack");
				exit(2);
			}
		}
	}

		//el otro caso es que lo que llega es un numero de secuencia mas alto de lo que estaba esperando pero eso no deberia pasar

	//FLO_: Nose porque silenciaste estop
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