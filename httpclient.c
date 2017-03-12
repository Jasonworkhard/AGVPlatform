/**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "httpclient.h"

/******************************************** 
功能：搜索字符串右边起的第一个匹配字符 
********************************************/ 
static char * Rstrchr(char* s, char x)    
{ 
	int i = strlen(s); 
	if(!(*s)) 
	{
		return 0; 
	}
	while(s[i-1])  
	{
		if(strchr(s+(i-1), x))     
		{
			return (s+(i-1));    
		}
		else   
		{
			i--;
		}
	}
	return 0; 
} 

/******************************************** 
功能：把字符串转换为全小写 
********************************************/ 
static void ToLowerCase(char* s)    
{ 
	while(*s && *s!='\0' )    
	{
		*s=tolower(*s++); 
	}
	*s = '\0';
} 

/************************************************************** 
功能：从字符串src中分析出网站地址和端口，并得到用户要下载的文件 
***************************************************************/ 
static void GetHost(char* src, char* web, char* file, int* port)    
{ 
	char* pA; 
	char* pB; 
	memset(web, 0, sizeof(web)); 
	memset(file, 0, sizeof(file)); 
	*port = 0; 
	if(!(*src))  
	{
		return; 
	}
	pA = src; 
	if(!strncmp(pA, "http://", strlen("http://")))   
	{
		pA = src+strlen("http://"); 
	}
	else if(!strncmp(pA, "https://", strlen( "https://")))     
	{
		pA = src+strlen( "https://"); 
	}
	pB = strchr(pA, '/'); 
	if(pB)     
	{ 
		memcpy(web, pA, strlen(pA)-strlen(pB)); 
		if(pB+1)   
		{ 
			memcpy(file, pB+1, strlen(pB)-1); 
			file[strlen(pB)-1] = 0; 
		} 
	} 
	else     
	{
		memcpy(web, pA, strlen(pA)); 
	}
	if(pB)
	{
		web[strlen(pA) - strlen(pB)] = 0; 
	}
	else     
	{
		web[strlen(pA)] = 0; 
	}
	pA = strchr(web, ':'); 
	if(pA)    
	{
		*port = atoi(pA + 1); 
		web[strlen(web)-strlen(pA)] = 0;
	}
	else   
	{
		*port = 80; 
	}
} 

/********************************************************************* 
*filename:   httpclient.c 
*purpose:   HTTP协议客户端程序，可以用来下载网页 
@ web-address
*********************************************************************/ 
int gethttpstring( char * web_address,unsigned char *httpbackstring )
{ 
	int sockfd = 0; //连接服务器socket
	int portnumber = 0;//端口号
	struct hostent *host;//连接主机地址
	struct sockaddr_in server_addr; //连接信息
	
	char buffer[1024] = "";//接收回复缓冲
	int nbytes = 0; //接收回复数据长度
	char host_addr[256] = ""; //解析后网址
	char host_file[1024] = ""; //解析后文件名
	char request[1024] = ""; //重组后http请求字符串
	int send = 0;
	int totalsend = 0; 
	int i = 0; 
	char *pt; 

	memset( httpbackstring, 0, 40);//sizeof(httpbackstring) ); 

	//printf( "parameter.1 is: %s\n ", web_address); 
	//ToLowerCase(web_address);/*将参数转换为全小写*/ 
	//printf( "lowercase   parameter.1   is:   %s\n ",   web_address); 
	GetHost( web_address, host_addr, host_file, &portnumber );/*分析网址、端口、文件名等*/ 
	//printf( "webhost:%s\n ", host_addr); 
	//printf( "hostfile:%s\n ", host_file); 
	//printf( "portnumber:%d\n\n ", portnumber); 

	if((host=gethostbyname(host_addr)) == NULL)/*取得主机IP地址*/ 
	{ 
		fprintf(stderr, "Http Gethostname error, %s\n ",   strerror(h_errno)); 
		return 0; 
	} 

	/*   客户程序开始建立   sockfd描述符   */ 
	if((sockfd=socket(AF_INET,SOCK_STREAM,0)) == -1)/*建立SOCKET连接*/ 
	{ 
		fprintf(stderr, "Http Socket Creat Error:%s\a\n ",strerror(errno)); 
		return 0; 
	} 

	/*   客户程序填充服务端的资料   */ 
	bzero(&server_addr,sizeof(server_addr)); 
	server_addr.sin_family=AF_INET; 
	server_addr.sin_port=htons(portnumber); 
	server_addr.sin_addr=*((struct in_addr*)host->h_addr); 

	/*   客户程序发起连接请求   */ 
	if(connect(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr)) == -1)/*连接网站*/ 
	{ 
		fprintf(stderr, "Connect Http Server Error:%s\a\n ",strerror(errno)); 
		return 0; 
	} 

	sprintf( request,"GET /%s HTTP/1.1\r\nAccept:*/*\r\nAccept-Language:zh-cn\r\nUser-Agent:Mozilla/4.0(compatible;MSIE 5.01;Windows NT 5.0)\r\nHost: %s:%d\r\nConnection: Close\r\n\r\n ", host_file, host_addr, portnumber); 
	//printf( "%s\n", request);/*准备request，将要发送给主机*/ 

	/*取得真实的文件名*/ 
	if(host_file && *host_file)     
	{
		pt = Rstrchr(host_file, '/'); 
	}
	else   
	{
		pt = 0; 
	}

	/*发送http请求request*/ 
	send = 0;
	totalsend = 0; 
	nbytes=strlen(request); 
	while(totalsend < nbytes)  
	{ 
		send = write(sockfd, request+totalsend, nbytes-totalsend); 
		if(send == -1)     
		{
			printf( "Send Http Request Error!%s\n ", strerror(errno));
			return 0; 
		} 
		totalsend += send;
		//printf("%d bytes send OK!\n ", totalsend); 
	} 
	//printf( "\nThe following is the http response:\n "); 
	//printf( "\nThe following is the http response:\n ");
	i=0; 
	int start=0;
	unsigned char recvtemp[1024]="";
	int recvvaluelen=0;
	/*   连接成功了，接收http响应，response   */ 
	while((nbytes=read(sockfd,buffer,1))==1) 
	{ 
		//printf( "%c ", buffer[0]);
		if(start == 2)//接收value值
		{
			if(buffer[0] == ',')
			{
				start = 0;
			}
			else
			{
				httpbackstring[recvvaluelen] = buffer[0];
				recvvaluelen++;
			}
		}
		if(start == 1)
		{
			if(buffer[0] == ':')//后面开始为回复数据区域
			{
				start = 2;
			}
		}
		if(buffer[0] == '{')//后面开始为回复数据区域
		{
			start=1;
		}
		if(buffer[0] == '}')//回复数据结束
		{
			break;
		}
	}
	//printf( "\n" );
	close(sockfd);
	return recvvaluelen; 
} 
