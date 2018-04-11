#include <stdlib.h>
#include <stdio.h>
//Threads
#include <unistd.h>
#include <pthread.h>
//Socket
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
//Timer
#include <time.h>


//Tamanho do buffer de leitura
#define TAM_BUFF (int) 10

//numero de nanosegundos por segundo
#define NSEC_PER_SEC    (1000000000)

//Define mensagens de leitura da temperatura
#define TA_MSG (char *) "sta0"
#define T_MSG  (char *) "st-0"
#define TI_MSG (char *) "sti0"

//Define mensagens de leitura de nível
#define NO_MSG (char *) "sno0"
#define H_MSG  (char *) "sh-0"

//Define mensgens de escrita de calor e fluxo
#define NI_MSG (char *) "ani" //+float e.g. "ani123.4"
#define Q_MSG  (char *) "aq-" //+float e.g. "aq-567.8"

//Define valores máximos para as variáveis
//temperaturas - ALARME
#define T_MAX  (float) 30.0
#define T_MIN  (float) 15.0

//fluxo - CONTROLE
#define NI_MAX (float) 100.0
#define NI_MIN (float) 0.0

//nivel - ALARME
#define H_MAX (float) 3.0
#define H_MIN (float) 1.0

//aquecedor - CONTROLE
#define Q_MAX (float) 1000000.0
#define Q_MIN (float) 0.1

//Define os periodos de repetição das threads
#define ALARM_MS   (int) 10000000000000
#define LOGRD_MS   (int) 10000000000000
#define LOGWR_MS   (int) 10000000000000
#define INPUT_MS   (int) 5000000000000
#define PRINT_MS   (int) 10000000000000 //1seg
#define HMEAS_MS   (int) 5000000000000
#define TMEAS_MS   (int) 5000000000000
#define HCTRL_MS   (int) 5000000000000
#define TCTRL_MS   (int) 5000000000000


#define TRUE        (int) 1
#define FALSE       (int) 0

//Define protótipos das funções respectivas as threads
void *periodic_thread(void * arg);
void t_alarm (void *alarm);
void t_logrd (void *logrd);
void t_logwr (void *logwr);
void t_print (void *print);
void t_input (void *input);
void t_hmeas (void *hmeas);
void t_hctrl (void *hcrl);
void t_tmeas (void *tmeas);
void t_tctrl (void *tctrl);

char **msg = (char *[]){"thread_logrd", "thread_logwr", "thread_input", "thread_hmeas", "thread_tmeas", "thread_print","thread_hctrl", "thread_tctrl", "thread_alarm"};


//Define os prototipos de funcao de acesso protegido
struct hmeas_t read_hmeas  (void);
void 					 write_hmeas (float no, float h);
struct tmeas_t read_tmeas  (void);
void 					 write_tmeas (float ta, float t, float ti);
float 				 read_ni		 (void);
void 					 write_ni		 (float new_ni);
float 				 read_q			 (void);
void 					 write_q		 (float new_q);
float 				 read_href   (void);
void 					 write_href	 (float new_href);
float 				 read_tref	 (void);
void 					 write_tref	 (float new_tref);

//Define protótipos das funções do socket
int create_local_socket(void);
struct sockaddr_in create_receiver_address(char *destino, int porta_destino);
void send_message(int local_socket, struct sockaddr_in endereco_destino, char *mensagem);
int receive_message(int local_socket, char *buffer, int TAM_BUFFER);

float get_value_from_socket(void *arg, char *command);
void send_value_through_socket(void *arg, char *command, float value);


typedef void (*generic_thread)(void *ptr);

typedef struct socket_info_t {
	int has_socket;
	int local_socket;
	struct sockaddr_in serv_addr;
	char *msg;
} socket_info_t;

typedef struct time_thread_t {
	int interval;
	generic_thread thread;
	struct socket_info_t socket_info;
} time_thread_t;

typedef struct tmeas_t {
	float ta;
	float t;
	float ti;	
} tmeas_t;

typedef struct hmeas_t {
	float no;
	float h;
} hmeas_t;

//Variáveis globais
float ni = 0.0;														//fluxo atual de agua
float q  = 0.0;												  	//aquecimento atual
float href = 2.3;													//referencia para nivel
float tref = 22.0;													//referencia para temperatura
struct tmeas_t tmeas = {0.0, 22.0, 0.0}; //estrutura com os dados de temperatura
struct hmeas_t hmeas = {0.0, 2.3}; 			//estrutura com os dados de nível

float tctrl_hist = 0.2;
float hctrl_hist = 0.2;

//Buffer
#define TAM_DOUBLE_BUFFER (int) 10
char *buffer1[TAM_DOUBLE_BUFFER];
char *buffer2[TAM_DOUBLE_BUFFER];
int current_pos = 0;
int current_buf = 0;

//Alarme
int ALARM_GO_ON = 1; 

//Configurações do servidor socket UDP
#define BASE_ADDRESS (char *) "ultrabook"
int porta_destino = 4545;


pthread_mutex_t mtx_hmeas;
pthread_mutex_t mtx_tmeas;
pthread_mutex_t mtx_alarm;
pthread_mutex_t mtx_log;
pthread_mutex_t mtx_term;

pthread_mutex_t mtx_ni;
pthread_mutex_t mtx_q;
pthread_mutex_t mtx_tref;
pthread_mutex_t mtx_href;

void mutex_init(void) {
	pthread_mutex_init(&mtx_hmeas, NULL);
  pthread_mutex_init(&mtx_tmeas, NULL);
  pthread_mutex_init(&mtx_alarm, NULL);
  pthread_mutex_init(&mtx_log,   NULL);
  pthread_mutex_init(&mtx_ni,    NULL);
  pthread_mutex_init(&mtx_q,     NULL);
  pthread_mutex_init(&mtx_href,  NULL);
  pthread_mutex_init(&mtx_tref,  NULL);
  pthread_mutex_init(&mtx_term,  NULL);
}


pthread_cond_t full_buff = PTHREAD_COND_INITIALIZER;
pthread_cond_t go_on    = PTHREAD_COND_INITIALIZER;


int main(int argc, char *argv[])
{
	mutex_init();
	pthread_t threads[9];
	int ret[9];

	struct time_thread_t tt0 = {LOGRD_MS,   t_logrd,  {FALSE, 0, {0, 0}, msg[0]}};
	struct time_thread_t tt1 = {0,   				t_logwr,  {FALSE, 0, {0, 0}, msg[1]}};
	struct time_thread_t tt2 = {INPUT_MS,   t_input,  {FALSE, 0, {0, 0}, msg[2]}};
	struct time_thread_t tt3 = {HMEAS_MS,   t_hmeas,  {TRUE,  0, {0, 0}, msg[3]}};
	struct time_thread_t tt4 = {TMEAS_MS,   t_tmeas,  {TRUE,  0, {0, 0}, msg[4]}};
	struct time_thread_t tt5 = {PRINT_MS,   t_print,  {FALSE, 0, {0, 0}, msg[5]}};
	struct time_thread_t tt6 = {HCTRL_MS,   t_hctrl,  {TRUE,  0, {0, 0}, msg[6]}};
	struct time_thread_t tt7 = {TCTRL_MS,   t_tctrl,  {TRUE,  0, {0, 0}, msg[7]}};
	struct time_thread_t tt8 = {0,				  t_alarm,  {TRUE,  0, {0, 0}, msg[8]}};

  ret[0] = pthread_create(&threads[0], NULL, periodic_thread, &tt0);
	ret[1] = pthread_create(&threads[1], NULL, (void *)t_logwr, &tt1);
	ret[2] = pthread_create(&threads[2], NULL, periodic_thread, &tt2);
	ret[3] = pthread_create(&threads[3], NULL, periodic_thread, &tt3);
	ret[4] = pthread_create(&threads[4], NULL, periodic_thread, &tt4);
	ret[5] = pthread_create(&threads[5], NULL, periodic_thread, &tt5);
	ret[6] = pthread_create(&threads[6], NULL, periodic_thread, &tt6);
	ret[7] = pthread_create(&threads[7], NULL, periodic_thread, &tt7);
	ret[8] = pthread_create(&threads[8], NULL, (void *)t_alarm, &tt8);

	printf("All threads UP!\n");
	
	pthread_join(threads[8], NULL);

	printf("All threads DOWN!\n");

	if(ret)
	{
		printf("\n thread logrd ret %d\n", ret[0]);
		printf("\n thread logwr ret %d\n", ret[1]);
		printf("\n thread input ret %d\n", ret[2]);
		printf("\n thread hmeas ret %d\n", ret[3]);
		printf("\n thread tmeas ret %d\n", ret[4]);
		printf("\n thread print ret %d\n", ret[5]);
		printf("\n thread hctrl ret %d\n", ret[6]);
		printf("\n thread tctrl ret %d\n", ret[7]);
		printf("\n thread alarm ret %d\n", ret[8]);
	} else	{ printf("\nApplication finished gracefully! \n");	}


	return 0;
}

void *periodic_thread(void *arg){

	struct time_thread_t *ptr_tt = (struct time_thread_t *) arg;
	struct time_thread_t tt = *ptr_tt;

	if(tt.socket_info.has_socket == TRUE){
		tt.socket_info.local_socket = create_local_socket();
		tt.socket_info.serv_addr = create_receiver_address(BASE_ADDRESS, porta_destino);
	}

 	struct timespec t;
  int interval = tt.interval;
	clock_gettime(CLOCK_MONOTONIC ,&t);
	t.tv_sec++;

	while(1){
		if(ALARM_GO_ON == 0) pthread_exit(NULL);

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
		tt.thread(&tt.socket_info);

		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
		       t.tv_nsec -= NSEC_PER_SEC;
		        t.tv_sec++;
  	}
  }
}

void t_logrd (void *arg)
{
	struct hmeas_t local_hmeas = read_hmeas();
	struct tmeas_t local_tmeas = read_tmeas();
	float local_ni = read_ni();
	float local_q  = read_q();
	float local_href = read_href();
	float local_tref = read_tref();

	char str[100];
	unsigned long current_time = (unsigned long)time(NULL);

	sprintf(str, "%lu;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;", current_time, local_hmeas.no, local_hmeas.h, local_tmeas.ta, local_tmeas.t, local_tmeas.ti, local_ni, local_q,
	local_href, local_tref);

	if(current_buf == 1){
		buffer1[current_pos] = str;
	} else if (current_buf == 0){
		buffer2[current_pos] = str;
	}
	current_pos++;

	if(current_pos >= TAM_DOUBLE_BUFFER){
		pthread_mutex_lock(&mtx_log);
		current_pos = 0;
		current_buf = (current_buf+1)%2;
    pthread_cond_signal(&full_buff);
   	pthread_mutex_unlock(&mtx_log);

	}

}

void t_logwr (void *arg)
{
	while(1){

		pthread_mutex_lock(&mtx_log);
		while( pthread_cond_wait(&full_buff, &mtx_log) != 0 )
		pthread_mutex_unlock(&mtx_log);

		if(ALARM_GO_ON == 0) {
			pthread_mutex_unlock(&mtx_log);
			pthread_exit(NULL);
		}

		FILE *f = fopen("log.txt", "ab");
		if (f == NULL)
		{
		    printf("Error opening file!\n");
		    exit(1);
		}

		for(int i = 0 ; i < TAM_DOUBLE_BUFFER; i++){
			if(current_buf == 1){
			 fprintf(f, "%s\n", buffer2[i]);
			} else if (current_buf == 0){
			 fprintf(f, "%s\n", buffer1[i]);
			}
		}

		fclose(f);
		pthread_mutex_unlock(&mtx_log);
	}
}

void t_print (void *arg)
{
	struct hmeas_t local_hmeas = read_hmeas();
	struct tmeas_t local_tmeas = read_tmeas();
	float local_ni = read_ni();
	float local_q  = read_q();
	float local_href = read_href();
	float local_tref = read_tref(); 

	pthread_mutex_lock(&mtx_term);

	printf("T= %.1f H= %.1f ",local_tmeas.t, local_hmeas.h);
	printf("Ni= %.1f Q= %.1f ", local_ni, local_q);
	printf("Tref= %.1f Href= %.1f \n",local_tref, local_href );

	pthread_mutex_unlock(&mtx_term);

	sleep(5);
}

void t_input (void *arg)
{
	int c;
	char str[TAM_BUFF];
	if(c = getchar() == '\n'){
		pthread_mutex_lock(&mtx_term);
		printf("\nTo change tref use: t+value. e.g. t19.3 , for 19.3 degrees \n");
		printf("To change href use: h+value. e.g. h2.1 , for  2.1 meters \n");
		printf("Enter here a new value: ");
		scanf("%s", str);
		while((c = getchar()) != '\n' && c != EOF)
		/* discard */ ;	
		
		char *decode = str;
		decode +=1;
		float value = strtof(decode, NULL);

		switch(str[0]){
			case 'T':
			case 't': write_tref(value);
								printf("Current tref set to: %.2f degrees\n", value);
								break;
			case 'H':
			case 'h':	write_href(value);
								printf("Current href set to: %.2f meters\n", value);
							  break;
		  default:
								printf("Command not recognized. 	\n");



		}

		pthread_mutex_unlock(&mtx_term);
	}

}

void t_hmeas (void *arg)
{
	float new_no, new_h;

  new_no = get_value_from_socket(arg, NO_MSG);
  new_h  = get_value_from_socket(arg, H_MSG);

  if(new_h > H_MAX | new_h < H_MIN){
  	//alarme
    pthread_cond_signal(&go_on);

  }

  write_hmeas(new_no, new_h);

}

void t_tmeas (void *arg)
{
	float new_ta, new_t, new_ti;

  new_ta = get_value_from_socket(arg, TA_MSG);
  new_t = get_value_from_socket(arg, T_MSG);
  new_ti = get_value_from_socket(arg, TI_MSG);
  
  if(new_t > T_MAX | new_t < T_MIN){
  	//alarme
    pthread_cond_signal(&go_on);

  }

  write_tmeas(new_ta, new_t, new_ti);

}

void t_hctrl (void *arg)
{
  
	struct hmeas_t local_hmeas = read_hmeas();
	float local_href = read_href();
	float new_ni;

	if(local_hmeas.h < local_href + hctrl_hist){
		//bomba
		new_ni = NI_MAX;
	} else if (local_hmeas.h > local_href - hctrl_hist){
    //nao bomba
    new_ni = NI_MIN;
	}
	send_value_through_socket(arg, NI_MSG, new_ni);

	write_ni(new_ni);
}

void t_tctrl (void *arg)
{
	
	struct tmeas_t local_tmeas = read_tmeas();
	float local_tref = read_tref();
	float new_q;

	if(local_tmeas.t < local_tref + tctrl_hist){
		//aquece
		new_q = Q_MAX;
	} else if (local_tmeas.t > local_tref - tctrl_hist){
		//nao aquece
		new_q = Q_MIN;
	}
	send_value_through_socket(arg, Q_MSG, new_q);

	write_q(new_q);

	
}

void t_alarm (void *arg){

	struct time_thread_t *ptr_tt = (struct time_thread_t *) arg;
	struct time_thread_t tt = *ptr_tt;

	if(tt.socket_info.has_socket == TRUE){
		tt.socket_info.local_socket = create_local_socket();
		tt.socket_info.serv_addr = create_receiver_address(BASE_ADDRESS, porta_destino);
	}

	pthread_mutex_lock(&mtx_alarm);
	while( pthread_cond_wait(&go_on, &mtx_alarm) != 0 )
		pthread_mutex_unlock(&mtx_alarm);
	
	ALARM_GO_ON = 0;
	pthread_mutex_unlock(&mtx_alarm);
	
	send_value_through_socket(&tt.socket_info, Q_MSG,  0.0);
	send_value_through_socket(&tt.socket_info, NI_MSG, 0.0);

	printf("\nAlarm went off!!! WARNING!!!\n");

}

void send_value_through_socket(void *arg, char *command, float value){
	
	struct socket_info_t *ptr_socket= (struct socket_info_t *) arg;
	struct socket_info_t socket = *ptr_socket;

	char msg[TAM_BUFF];
  char float_str[TAM_BUFF];

  strcpy(msg, command);
  sprintf(float_str, "%.1f", value);
  strcat(msg, float_str);

	send_message(socket.local_socket, socket.serv_addr, msg);	
}


float get_value_from_socket(void *arg, char *command){
	
	struct socket_info_t *ptr_socket= (struct socket_info_t *) arg;
	struct socket_info_t socket = *ptr_socket;
	char msg_recebida[TAM_BUFF];
	
	send_message(socket.local_socket, socket.serv_addr, command);
	receive_message(socket.local_socket, msg_recebida, TAM_BUFF);
	
	char *decode = msg_recebida;
	decode += 3;
	return strtof(decode, NULL);
}



int create_local_socket(void)
{
	int local_socket;		/* Socket usado na comunicacão */

	local_socket = socket( PF_INET, SOCK_DGRAM, 0);
	if (local_socket < 0) {
		perror("socket");
		return -1;
	}
	return local_socket;
}


struct sockaddr_in create_receiver_address(char *destino, int porta_destino)
{
	struct sockaddr_in servidor; 	/* Endereço do servidor incluindo ip e porta */
	struct hostent *dest_internet;	/* Endereço destino em formato próprio */
	struct in_addr dest_ip;		/* Endereço destino em formato ip numérico */

	if (inet_aton ( destino, &dest_ip ))
		dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
	else
		dest_internet = gethostbyname(destino);

	if (dest_internet == NULL) {
		fprintf(stderr,"Endereço de rede inválido\n");
		exit(1);
	}

	memset((char *) &servidor, 0, sizeof(servidor));
	memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(porta_destino);

	return servidor;
}

void send_message(int local_socket, struct sockaddr_in endereco_destino, char *mensagem)
{
	/* Envia msg ao servidor */

	if (sendto(local_socket, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{ 
		perror("sendto");
		return;
	}
}


int receive_message(int local_socket, char *buffer, int TAM_BUFFER)
{
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(local_socket, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}


struct hmeas_t read_hmeas (void){
	struct hmeas_t aux;
  //lock
  pthread_mutex_lock(&mtx_hmeas);
	aux = hmeas;
 //unlock
  pthread_mutex_unlock(&mtx_hmeas);	
	return aux;
}

void write_hmeas (float no, float h){
	//lock
  pthread_mutex_lock(&mtx_hmeas);
	hmeas.no = no;
	hmeas.h  = h;
	//unlock
  pthread_mutex_unlock(&mtx_hmeas);

}

struct tmeas_t read_tmeas (void){
	struct tmeas_t aux;
	//lock
  pthread_mutex_lock(&mtx_tmeas);
	aux = tmeas;
	//unlock
  pthread_mutex_unlock(&mtx_tmeas);
	return aux;
}

void write_tmeas(float ta, float t, float ti){
	//lock
  pthread_mutex_lock(&mtx_tmeas);
	tmeas.ta = ta;
	tmeas.t  = t;
	tmeas.ti = ti;
	//unlock
  pthread_mutex_unlock(&mtx_tmeas);
}

float read_ni(void){
	float aux;
	//lock
  pthread_mutex_lock(&mtx_ni);
	aux = ni;
	//unlock
  pthread_mutex_unlock(&mtx_ni);
	return aux;
}

void write_ni(float new_ni){
	//lock
  pthread_mutex_lock(&mtx_ni);
	ni = new_ni;
	//unlock
	pthread_mutex_unlock(&mtx_ni);
}

float read_q(void){
	float aux;
	//lock
  pthread_mutex_lock(&mtx_q);
	aux = q;
	//unlock
  pthread_mutex_unlock(&mtx_q);
	return aux;
}

void write_q(float new_q){
	//lock
	pthread_mutex_lock(&mtx_q);
	q = new_q;
	//unlock
  pthread_mutex_unlock(&mtx_q);
}

float read_href(void){
	float aux;
	//lock
	pthread_mutex_lock(&mtx_href);
	aux = href;
	//unlock
  pthread_mutex_unlock(&mtx_href);
	return aux;
}

void write_href(float new_href){
	//lock
	pthread_mutex_lock(&mtx_href);
	href = new_href;
	//unlock
  pthread_mutex_unlock(&mtx_href);
}

float read_tref(void){
	float aux;
	//lock  
	pthread_mutex_lock(&mtx_tref);
	aux = tref;
	//unlock
  pthread_mutex_unlock(&mtx_tref);
	return aux;
}

void write_tref(float new_tref){
	//lock
  pthread_mutex_lock(&mtx_tref);
	tref = new_tref;
	//unlock
  pthread_mutex_unlock(&mtx_tref);
}







