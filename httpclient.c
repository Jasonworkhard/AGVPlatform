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
���ܣ������ַ����ұ���ĵ�һ��ƥ���ַ� 
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
���ܣ����ַ���ת��ΪȫСд 
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
���ܣ����ַ���src�з�������վ��ַ�Ͷ˿ڣ����õ��û�Ҫ���ص��ļ� 
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
*purpose:   HTTPЭ��ͻ��˳��򣬿�������������ҳ 
@ web-address
*********************************************************************/ 
int gethttpstring( char * web_address,unsigned char *httpbackstring )
{ 
	int sockfd = 0; //���ӷ�����socket
	int portnumber = 0;//�˿ں�
	struct hostent *host;//����������ַ
	struct sockaddr_in server_addr; //������Ϣ
	
	char buffer[1024] = "";//���ջظ�����
	int nbytes = 0; //���ջظ����ݳ���
	char host_addr[256] = ""; //��������ַ
	char host_file[1024] = ""; //�������ļ���
	char request[1024] = ""; //�����http�����ַ���
	int send = 0;
	int totalsend = 0; 
	int i = 0; 
	char *pt; 

	memset( httpbackstring, 0, 40);//sizeof(httpbackstring) ); 

	//printf( "parameter.1 is: %s\n ", web_address); 
	//ToLowerCase(web_address);/*������ת��ΪȫСд*/ 
	//printf( "lowercase   parameter.1   is:   %s\n ",   web_address); 
	GetHost( web_address, host_addr, host_file, &portnumber );/*������ַ���˿ڡ��ļ�����*/ 
	//printf( "webhost:%s\n ", host_addr); 
	//printf( "hostfile:%s\n ", host_file); 
	//printf( "portnumber:%d\n\n ", portnumber); 

	if((host=gethostbyname(host_addr)) == NULL)/*ȡ������IP��ַ*/ 
	{ 
		fprintf(stderr, "Http Gethostname error, %s\n ",   strerror(h_errno)); 
		return 0; 
	} 

	/*   �ͻ�����ʼ����   sockfd������   */ 
	if((sockfd=socket(AF_INET,SOCK_STREAM,0)) == -1)/*����SOCKET����*/ 
	{ 
		fprintf(stderr, "Http Socket Creat Error:%s\a\n ",strerror(errno)); 
		return 0; 
	} 

	/*   �ͻ�����������˵�����   */ 
	bzero(&server_addr,sizeof(server_addr)); 
	server_addr.sin_family=AF_INET; 
	server_addr.sin_port=htons(portnumber); 
	server_addr.sin_addr=*((struct in_addr*)host->h_addr); 

	/*   �ͻ���������������   */ 
	if(connect(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr)) == -1)/*������վ*/ 
	{ 
		fprintf(stderr, "Connect Http Server Error:%s\a\n ",strerror(errno)); 
		return 0; 
	} 

	sprintf( request,"GET /%s HTTP/1.1\r\nAccept:*/*\r\nAccept-Language:zh-cn\r\nUser-Agent:Mozilla/4.0(compatible;MSIE 5.01;Windows NT 5.0)\r\nHost: %s:%d\r\nConnection: Close\r\n\r\n ", host_file, host_addr, portnumber); 
	//printf( "%s\n", request);/*׼��request����Ҫ���͸�����*/ 

	/*ȡ����ʵ���ļ���*/ 
	if(host_file && *host_file)     
	{
		pt = Rstrchr(host_file, '/'); 
	}
	else   
	{
		pt = 0; 
	}

	/*����http����request*/ 
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
	/*   ���ӳɹ��ˣ�����http��Ӧ��response   */ 
	while((nbytes=read(sockfd,buffer,1))==1) 
	{ 
		//printf( "%c ", buffer[0]);
		if(start == 2)//����valueֵ
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
			if(buffer[0] == ':')//���濪ʼΪ�ظ���������
			{
				start = 2;
			}
		}
		if(buffer[0] == '{')//���濪ʼΪ�ظ���������
		{
			start=1;
		}
		if(buffer[0] == '}')//�ظ����ݽ���
		{
			break;
		}
	}
	//printf( "\n" );
	close(sockfd);
	return recvvaluelen; 
} 
