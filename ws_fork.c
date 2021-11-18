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

	exit(1);
}

void sigchld(int signo)
{
	pid_t pid;
	
	while((pid=waitpid(-1, NULL, WNOHANG))>0);
}

int main(int argc, char **argv)
{
	int listenfd, socketfd;
	pid_t pid;
	socklen_t length;
	struct sockaddr_in cli_addr, serv_addr;
	

//背景執行
	if(fork() != 0)
		return 0;

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

	signal(SIGCHLD, sigchld);

	while(1) {
		length = sizeof(cli_addr);
//等待客戶端連線
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length))<0)
			exit(3);

//fork()子行程
		if((pid = fork()) < 0) {
			exit(3);
		}else {
			if(pid == 0) { //子
				close(listenfd);
				handle_socket(socketfd);
				exit(0);
			}else { //父
				close(socketfd);
			}
		}
	}
	return 0;
}
