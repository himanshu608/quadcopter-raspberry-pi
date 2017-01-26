#include "serv.h"

void clean_args_SERVER(args_SERVER * arg) {
	if (arg != NULL) {
		if (arg->pmutexRemoteConnect != NULL) {
			clean_PMutex(arg->pmutexRemoteConnect);
		}
		free(arg);
		arg=NULL;
	}
}

char isMessage(char * messageReceve, char * messageToTest) {
	char str1[SIZE_SOCKET_MESSAGE];
	char str2[SIZE_SOCKET_MESSAGE];
	int res = 0;

	strcpy(str1, messageToTest);
	strcpy(str2, messageReceve);
	res = strcmp(str1, str2);

	return res == 0;
}

char isMessagePause(char * message) {
	return isMessage(message,"PAUSE");
}

char isMessageSTOP(char * message){
	return isMessage(message,"STOP");
}

void MessageToStruc(char * message,int sizeFloat,args_SERVER * arg){


	//PMutex * pmutex = arg->dataController->pmutex->mutex;


	int tmp=sizeFloat;

	float a=strtof(message,0);


	float b=strtof(message+tmp-1,0);

	tmp+=sizeFloat-1;

	float c=strtof(message+tmp-1,0);

	tmp+=sizeFloat-1;

	float d=strtof(message+tmp-1,0);

	pthread_mutex_lock(&arg->dataController->pmutex->mutex);


	arg->dataController->axe_Rotation=a;
	arg->dataController->axe_UpDown=b;
	arg->dataController->axe_LeftRight=c;
	arg->dataController->axe_FrontBack=d;

	if(arg->dataController->pmutex->var>0){
		arg->dataController->pmutex->var=1;
	}

	pthread_cond_signal(&(arg->dataController->pmutex->condition));

	pthread_mutex_unlock(&(arg->dataController->pmutex->mutex));

	if(arg->verbose){
	printf ("THREAD SERV : float a = %.6f  |float b = %.6f  |float c = %.6f  |float d = %.6f  |\n", a ,b,c,d);
	}
}


void *thread_UDP_SERVER(void *args) {

	args_SERVER *argSERV = (args_SERVER*) args;

	char verbose =argSERV->verbose;
	if(verbose){printf("THREAD SERV : SERVEUR UDP\n");}
	int sock;
	struct sockaddr_in adr_svr;

	memset(&adr_svr, 0, sizeof(adr_svr));
	adr_svr.sin_family 		= AF_INET;
	adr_svr.sin_addr.s_addr = htonl(INADDR_ANY);
	adr_svr.sin_port 		= htons(8888);

	if((sock=socket(PF_INET,SOCK_DGRAM,0)) ==-1 ){
		perror("THREAD SERV : Socket error");
	}

	if(bind(sock,(struct sockaddr *)&adr_svr,sizeof(adr_svr))){
		perror("THREAD SERV : bind error");
	}
	char buff[SIZE_SOCKET_MESSAGE];

	recvfrom(sock,buff,SIZE_SOCKET_MESSAGE-1, 0,NULL,NULL);
	buff[SIZE_SOCKET_MESSAGE-1] = '\0';
	if(verbose){printf("THREAD SERV : messag recu : %s\n",buff);}

	pthread_mutex_lock(&argSERV->pmutexRemoteConnect->mutex);
	pthread_cond_signal(&argSERV->pmutexRemoteConnect->condition);
	pthread_mutex_unlock(&argSERV->pmutexRemoteConnect->mutex);

	int fini = 1;
	int i=1;
	while(fini){
		recvfrom(sock,buff,SIZE_SOCKET_MESSAGE-1, 0,NULL,NULL);
		if(verbose){printf("THREAD SERV : messag recu %d : %s\n",i,buff);}
		i++;

		buff[SIZE_SOCKET_MESSAGE-1] = '\0';

		if(isMessagePause(buff)=='1'){
			if(verbose){
				printf("THREAD SERV : PAUSE MESSAGE\n");
			}
		}
		if (isMessageSTOP(buff)=='1') {
			if (verbose) {
				printf("THREAD SERV : STOP MESSAGE\n");
			}
			pthread_mutex_lock(&argSERV->dataController->pmutex->mutex);
			argSERV->dataController->flag=2;
			pthread_mutex_unlock(&argSERV->dataController->pmutex->mutex);
			fini=0;
		} else {
			MessageToStruc(buff, 10, argSERV);
		}

	}

	return NULL;
}

void *thread_TCP_SERVER(void *args) {

	args_SERVER *argSERV = (args_SERVER*) args;
	char verbose =argSERV->verbose;
	if(verbose){printf("THREAD SERV : SERVEUR TCP\n");}

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address_sock;
	address_sock.sin_family = AF_INET;
	address_sock.sin_port = htons(8888);

	printf("THREAD SERV : attente sur port : %d\n", 8888);

	address_sock.sin_addr.s_addr = htonl(INADDR_ANY);
	int r = bind(sock, (struct sockaddr *) &address_sock,
			sizeof(struct sockaddr_in));
	if (r != 0) {
		return NULL;
	}
	r = listen(sock, 0);
	int connect = 1;
	while (connect) {
		struct sockaddr_in caller;
		socklen_t size = sizeof(caller);

		int sock2 = accept(sock, (struct sockaddr *) &caller, &size);
		printf("THREAD SERV : SERVEUR ACCEPT\n");

		if (sock2 < 0) {
			printf("THREAD SERV : Bind fail");

		}



		pthread_mutex_lock(&argSERV->pmutexRemoteConnect->mutex);

		pthread_cond_signal(&argSERV->pmutexRemoteConnect->condition);

		pthread_mutex_unlock(&argSERV->pmutexRemoteConnect->mutex);


		char *mess = "HELLO\n";
		int result = write(sock2, mess, strlen(mess) * sizeof(char));

		//TODO do a WHILE SUR le WRITE

		fcntl(sock2, F_SETFL, O_NONBLOCK);
		int fd_max = sock2 + 1;
		struct timeval tv;
		fd_set rdfs;

		int fini = 1;
		while (fini) {
			FD_ZERO(&rdfs);
			FD_SET(sock2, &rdfs);
			tv.tv_sec = 3;
			tv.tv_usec = 500000;
			int ret = select(fd_max, &rdfs, NULL, NULL, &tv);
			//printf("valeur de retour de select : %d\n", ret);
			if (ret == 0) {
				printf("THREAD SERV : Timed out\n");
				fini = 0;
			} else if (FD_ISSET(sock2, &rdfs)) {
				int messageRead = 0;
				int iter = 0;
				char buff[SIZE_SOCKET_MESSAGE];
				while (messageRead < 1 && iter < 10) {
					iter++;
					//printf("THREAD SERV : try to read\n");
					int bytesRead = read(sock2, buff, SIZE_SOCKET_MESSAGE - messageRead);
					messageRead += bytesRead;
				}
				if (messageRead > 0) {
					buff[SIZE_SOCKET_MESSAGE-1] = '\0';
					//printf("THREAD SERV : Message recu : %s\n", buff);
					MessageToStruc(buff, 10, argSERV);

					char str1[2];
					char str2[2];
					int res = 0;
					strcpy(str1, "X\0");
					strcpy(str2, buff);
					res = strcmp(str1, str2);
					if (res == 0) {
						fini = 0;
						printf("THREAD SERV : c'est fini !!\n");
					}
				} else {
					printf("THREAD SERV : NOTHING TO READ !!\n");
					fini = 0;
				}
			} else {
				printf("THREAD SERV : SELECT EXIST WITHOUT GOOD VALUE\n");
			}
		}
		close(sock2);
	}
	return NULL;
}
