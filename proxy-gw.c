#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifdef __linux
#include <linux/netfilter_ipv4.h>
#endif

#define BUF_SIZE 65536

pid_t passthru(int sock1, int sock2, int share_id){
	pid_t pid = fork();
	if(pid < 0){
		perror("fork");
		exit(-1);
	}
	
	if(pid == 0){
		char *buf = malloc(BUF_SIZE);
		while(1){
			ssize_t received = recv(sock1, buf, BUF_SIZE, 0);
			if(received == 0) break;
			send(sock2, buf, received, 0);
		}
		
		shutdown(sock1, SHUT_RDWR);
		shutdown(sock2, SHUT_RDWR);
		
		pid_t *pids = shmat(share_id, NULL, SHM_RDONLY);
		kill(pids[0] == getpid() ? pids[1] : pids[0], SIGTERM);
		
		// printf("Close: %d, %d \n", pids[0], pids[1]);
		shmctl(share_id, IPC_RMID, 0);
		
		exit(0);
	}
	
	return pid;
}

int main(int argc, char** argv){
	if(argc < 2){
		puts("Listen port was not specified.");
		exit(-1);
	}
	
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(atoi(argv[1]));
	saddr.sin_addr.s_addr = INADDR_ANY;
	
	int ssock = socket(AF_INET, SOCK_STREAM, 0);
	if(ssock < 0){
		perror("socket");
		exit(-1);
	}
	
	int true = 1;
	if(setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int))){
		perror("setsockopt");
		exit(-1);
	}
	
	if(bind(ssock, (struct sockaddr *)&saddr, sizeof(saddr))){
		perror("bind");
		exit(-1);
	}
	
	if(listen(ssock, 5)){
		perror("listen");
		exit(-1);
	}
	
	printf("Listen: %s \n", argv[1]);
	
	while(1){
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		
		int sock = accept(ssock, (struct sockaddr *)&addr, &len);
		if(sock < 0){
			perror("accept");
			exit(-1);
		}
		
		char addr_str[32];
		sprintf(addr_str, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		
		struct sockaddr_in caddr;
		#ifdef __linux
		if(getsockopt(sock, IPPROTO_IP, SO_ORIGINAL_DST, &caddr, &len)){
			perror("getsockopt");
			continue;
		}
		#else
		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(80);
		caddr.sin_addr.s_addr = inet_addr("163.44.171.5");
		#endif
		
		int csock = socket(AF_INET, SOCK_STREAM, 0);
		if(csock < 0){
			perror("socket");
			exit(-1);
		}
		
		if(connect(csock, (struct sockaddr *)&caddr, sizeof(caddr))){
			perror("connect");
			exit(-1);
		}
		
		printf("Establish: %s -> %s:%d \n", addr_str, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
		
		pid_t pid = fork();
		if(pid < 0){
			perror("fork");
			exit(-1);
		}
		
		if(pid == 0){
			int share_id = shmget(IPC_PRIVATE, 2 * sizeof(pid_t), 0600);
			if(share_id < 0){
				perror("shmget");
				exit(-1);
			}
			
			pid_t *pids = shmat(share_id, NULL, 0);
			pids[0] = passthru(sock, csock, share_id);
			pids[1] = passthru(csock, sock, share_id);
			
			// printf("Open: %d, %d \n", pids[0], pids[1]);
			exit(0);
		}
		
		wait(NULL);
	}
	
	close(ssock);
	return 0;
}
