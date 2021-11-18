#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define SERV_PORT 80
#define BUFSIZE 8096

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpeg"},
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"exe","text/plain" },
	{0,0} };

void handle_socket(int fd)
{
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;
	static char buffer[BUFSIZE+1];

//讀取瀏覽器要求
	ret = read(fd,buffer,BUFSIZE);
//網路炸裂，結束行程
	if(ret==0||ret==-1) {
		exit(3);
	}
//尾巴補\0
	if(ret>0&&ret<BUFSIZE)
		buffer[ret] = 0;
	else
		buffer[0] = 0;
//刪\r\n
	for(i=0;i<ret;i++) 
		if(buffer[i]=='\r'||buffer[i]=='\n')
			buffer[i] = 0;
//判斷GET命令
	if(strncmp(buffer,"GET ",4)&&strncmp(buffer,"get ",4))
		exit(3);
//分隔HTTP/1.0
	for(i=4;i<BUFSIZE;i++) {
		if(buffer[i] == ' ') {
			buffer[i] = 0;
			break;
		}
	}
//當客戶端要求根目錄時讀取 index.html
	if(!strncmp(&buffer[0],"GET /\0",6)||!strncmp(&buffer[0],"get /\0",6) )
		strcpy(buffer,"GET /index.html\0");
//檢查客戶端所要求的檔案格式
	buflen = strlen(buffer);
	fstr = (char *)0;

	for(i=0;extensions[i].ext!=0;i++) {
		len = strlen(extensions[i].ext);
		if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr = extensions[i].filetype;
			break;
		}
	}
//檔案格式不支援
	if(fstr == 0) {
		fstr = extensions[i-1].filetype;
	}
//開檔
	if((file_fd=open(&buffer[5],O_RDONLY))==-1)
		write(fd, "Failed to open file", 19);
//回傳 200OK 內容格式
	sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
	write(fd,buffer,strlen(buffer));
//讀檔，輸出到客戶端
	while((ret=read(file_fd, buffer, BUFSIZE))>0) {
		write(fd,buffer,ret);
	}
}

int main(int argc, char **argv)
{
	int i, maxi, maxfd;
	int listenfd, socketfd, connfd, sockfd;
	int nready, client[FD_SETSIZE];
	ssize_t n;
	fd_set rset, allset;
	char buf[BUFSIZE];
	socklen_t length;
	struct sockaddr_in cli_addr, serv_addr;

//開啟網路socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
//網路連線設定
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT); //port80
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //使用任何在本機的對外IP
//開啟網路監聽器
	bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
//開始監聽
	listen(listenfd, 64);

//初始化
	maxfd = listenfd;
	maxi = -1;
	for(i=0; i<FD_SETSIZE; i++)
		client[i] = -1;
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);

	while(1) {
		rset = allset;
		if((nready=select(maxfd+1, &rset, NULL, NULL, NULL))<0)
			exit(4);

//等待客戶端連線
		if(FD_ISSET(listenfd, &rset)) {
			length = sizeof(cli_addr);
			connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);

			for(i=0; i<FD_SETSIZE; i++)
				if(client[i] < 0) {
					client[i] = connfd;
					break;
				}
			if(i == FD_SETSIZE)
				exit(3);

			FD_SET(connfd, &allset);
			if(connfd > maxfd)
				maxfd = connfd;
			if(i > maxi)
				maxi = i;

			if(--nready <= 0)
				continue;
		}
//對現有連線進行讀寫
		for(i=0; i<=maxi; i++) {
			if((sockfd=client[i]) < 0)
				continue;
			if(FD_ISSET(sockfd, &rset)) {
				handle_socket(sockfd);
				close(sockfd);
				FD_CLR(sockfd, &allset);
				client[i] = -1;
				
				if(--nready <= 0)
					break;
			}
		}
	}
	return 0;
}
