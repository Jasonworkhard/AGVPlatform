#include <sys/types.h>
#include <stdio.h>/*��׼�����������*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>/*Unix ��׼��������*/
#include <stdlib.h>/*��׼�����ⶨ��*/
#include <pthread.h>
#include <termios.h>/*PPSIX �ն˿��ƶ���*/
#include <fcntl.h>/*�ļ����ƶ���*/
#include <arpa/inet.h>/*inet_ntoa*/
#include <string.h>/*strcpy*/
#include <semaphore.h>//sem 
#include "debug.h"
#include "serial.h"
#include "operateini.h"
#include "httpclient.h"

/* baudrate settings are defined in <asm/termbits.h>, which is included by <termios.h> */
#define BAUDRATE 9600

//�ڵ���Ϣ�����ļ�·��
#define NODECONFIG_INI_PATH "./config.ini"

//���ڵ���
#define NODEMAXNUMBER 100

//http�����ַ
#define HTTPWEBADDRONE "http://115.24.161.178:8080/SmartHomeBack/ajax/FacilitySignalAction.action?code="
#define HTTPWEBADDRTWO "&signal="
#define HTTPWEBADDRTEST "http://115.24.161.158:8080/DCPDispatcher/ajax/DispatchHeartPhoneAction.action"

//����MAC��ַ
#define MAC "ff000001"

//
#define SOCKIPLEN 16
#define BACKLOG 10
#define TRUE 1
#define FALSE 0
#define BYTE unsigned char

typedef struct _sock_attr{
	char *s_port;					//�����˿ں�
	fd_set clientsock_fds;
	int clientsock_attr[FD_SETSIZE];		//���socket���
	int clientsock_num;				//�ͻ���������
	int clientsock_timeout[FD_SETSIZE];//�ͻ���socket��ʱ��
	char clientsock_ipattr[FD_SETSIZE][SOCKIPLEN];	//�ͻ���ip��ַ
	unsigned char clientsock_type[FD_SETSIZE];//�ͻ�������0���Ƿ��û� 1��ǰ̨��ʾ���� 2��ң��ģ�� 3���Ž�ģ��
}*pSOCK_ATTR,SOCK_ATTR;

//��������
void * thread_socket(void *arg);
void * thread_serial(void *arg);
void * thread_timingacquisition(void *arg);
void * thread_timingrequesthttp(void *arg);
static void perform_commond( int fd,char *commond_buf,int commond_len );

#define NODEIDATTRLEN 3
#define NODEVALUEATTRLEN 20
//��������
static pSOCK_ATTR sendtosocket;
static int serialhandle;
static BYTE node_id[NODEMAXNUMBER][NODEIDATTRLEN] = { 0 }; 
static BYTE node_value[NODEMAXNUMBER][NODEVALUEATTRLEN] = { 0 };
static int node_number = 0;
static sem_t sendcontrolcommand_sem;


/**
@maintenancesockattr : ά���ͻ���sock���к���
@
@int fd : ��رյ�sock���
@void* arg : ��ά����sock����ָ��
@char type : �������� 'a'==add 'r'==remove 'u'==update
**/
static void maintenancesockattr( int fd, void* arg, char type )
{
	int i,j,n;
	pSOCK_ATTR sock=(pSOCK_ATTR)arg;//struct of diy_sock
	if(type=='r')//remove
	{
		if(close(fd)==0)
		{
			FD_CLR(fd,&sock->clientsock_fds);
			for(i=0;i<sock->clientsock_num;i++)
			{
				if(fd==sock->clientsock_attr[i])
				{
					for(j=i;j<sock->clientsock_num-1;j++)
					{
						sock->clientsock_attr[j]=sock->clientsock_attr[j+1];
						sock->clientsock_type[j]=sock->clientsock_type[j+1];
						sock->clientsock_timeout[j]=sock->clientsock_timeout[j+1];
						for(n=0;n<SOCKIPLEN;n++)
						{
							sock->clientsock_ipattr[j][n]=sock->clientsock_ipattr[j+1][n];
						}
					}
					i=sock->clientsock_num;//==break;
				}
			}
			DBG("Removing client on fd %d\n",fd);
			sock->clientsock_num--;
			DBG("Current client numbers : %d\n",sock->clientsock_num);
		}
	}
	else if(type=='u')//update
	{
		for(i=0;i<sock->clientsock_num;i++)
		{
			if(fd==sock->clientsock_attr[i])
			{
				sock->clientsock_timeout[i]=0;
				i=sock->clientsock_num;//==break;
			}
		}
	}
	else if(type=='a')//add
	{
		int client_len,client_sockfd;
		struct sockaddr_in client_addr;
		char *ip;
		client_len=sizeof(client_addr);
		//��ÿͻ�������socket
		client_sockfd=accept(fd,(struct sockaddr *)&client_addr,&client_len);
		if(client_sockfd==-1)
		{
			DBG("Accept err\n");
		}
		else
		{
			//�����������
			FD_SET(client_sockfd,&sock->clientsock_fds);
			sock->clientsock_attr[sock->clientsock_num]=client_sockfd;//�洢�ͻ���sock
			sock->clientsock_timeout[sock->clientsock_num]=0;//��ʼ���ͻ��˳�ʱ��
			sock->clientsock_type[sock->clientsock_num] = 0;//��ʼ���ͻ�����������
			ip=inet_ntoa(client_addr.sin_addr);
			memset(sock->clientsock_ipattr[sock->clientsock_num],0,16);
			strcpy(sock->clientsock_ipattr[sock->clientsock_num],ip);//�洢�ͻ���ip
			//DBG("Adding client on fd %d\n",client_sockfd);
			sock->clientsock_num++;
			//DBG("Current client numbers : %d\n",sock->clientsock_num);
		}
	}
}

/**
@initdbglog : ��ʼ��������־
@
@char* path : ��־���·��
**/
void initdbglog( char* path )
{
	debug_term_on();
	debug_set_dir( path );
	debug_file_on();
}

//��ascll��תʮ������ 
//�� 'a'->0x0a '1'->0x01
static unsigned char atoh( unsigned char a )
{
	if(a>='0' && a<='9')
	{
		a = a-'0';
	}
	else if(a>='a' && a<='f')
	{
		a = a-'a'+0x0a;
	}
	else if(a>='A' && a<='F')
	{
		a = a-'A'+0x0a;
	}
	return a;
}

/**
@loadnodeconf : ����ڵ���Ϣ
@id ��ʮ�����Ʒ�ʽ�洢�����ļ��ڽڵ�ID
@value���ַ�����ʽ�洢�����ļ��ڽڵ�ID
@number���洢�ڵ�����
**/
static int loadnodeconf( BYTE (*id)[NODEIDATTRLEN],BYTE (*value)[NODEVALUEATTRLEN],int *number )
{
	BYTE tmpnodeid[NODEMAXNUMBER*10] = { 0 }; 
	//���ļ�����ڵ�ID
	*number = GetKeyString( "NODEID",NODECONFIG_INI_PATH,tmpnodeid ); 
	if( *number == 0 )
	{
		return 0;
	}
	int i = 0;   
	for(i=0;i<*number;i++)
	{
		id[i][0]=((atoh(tmpnodeid[i*10+0])<<4)&0xf0)|((atoh(tmpnodeid[i*10+1]))&0x0f);
		id[i][1]=((atoh(tmpnodeid[i*10+2])<<4)&0xf0)|((atoh(tmpnodeid[i*10+3]))&0x0f);
		id[i][2]=((atoh(tmpnodeid[i*10+4])<<4)&0xf0)|((atoh(tmpnodeid[i*10+5]))&0x0f);
		value[i][0]=0x24;
		value[i][1]=id[i][0];
		value[i][2]=id[i][1];
		value[i][3]=id[i][2];
		value[i][18]=0x0D;
		value[i][19]=0x0A;
		////printf the serail come data
		/*MSG("node %d id:",i);
		int j=0;
		for(j=0;j<6;j++)
		{
		MSG("%c ",tmpnodeid[i*10+j]);
		}
		MSG("\n");*/
	}
	return 1;
}

/**
@main : main����
**/
int main( int argc, char **argv )
{
	if(argc<3)
	{
		fprintf( stderr, "usage : %s socketlisten_port Serialdev_path.\n", argv[0] );
		exit( EXIT_FAILURE );
	}
	MSG("\n");
	MSG("         ****************************************************\n");
	MSG("         ********         SmartHome Gateway          ********\n");
	MSG("         ****************************************************\n");
	MSG("\n");
	initdbglog( "./log" );

	//init Semaphore
	if( sem_init( &sendcontrolcommand_sem, 0, 0 ) )
	{
		MSG("Semaphore initialization failed!\n");
		exit( EXIT_FAILURE );
	}

	//load node info
	DBG("<------------Load Node Info------------>\n");
	if( loadnodeconf( node_id,node_value,&node_number ) == 0 )
	{
		MSG("No Node Information !\n");
		exit( EXIT_FAILURE );
	}

	//
	DBG("<------------Service began------------>\n");

	//some variable
	int res,i;
	pthread_t tmp_thread;

	/*start serial service thread*/
	pthread_t b_thread;
	res = pthread_create( &b_thread,NULL,thread_serial,(void *)argv[2] );
	if( res != 0 )
	{
		MSG("Serial Service Thread Creation Failed!\n");
		exit( EXIT_FAILURE );
	}

	/*start socket service thread*/
	SOCK_ATTR sock;
	sendtosocket = &sock;
	sock.s_port = argv[1];
	pthread_t a_thread;
	res = pthread_create( &a_thread,NULL,thread_socket,(void *)&sock );
	if( res != 0 )
	{
		MSG("Socket Service Thread Creation Failed!\n");
		exit( EXIT_FAILURE );
	}

	/*start timing request httpserver thread*/
	res = pthread_create( &a_thread,NULL,thread_timingrequesthttp,NULL );
	if( res != 0 )
	{
		MSG("Timing Request Httpserver Thread Creation Failed!\n");
		exit( EXIT_FAILURE );
	}

	/*start timing gacquisition thread*/	
	res = pthread_create( &a_thread,NULL,thread_timingacquisition,NULL );
	if( res != 0 )
	{
		MSG("Timing Gacquisition Thread Creation Failed!\n");
		exit( EXIT_FAILURE );
	}

	//
	while(1)
	{
		sleep(10);
		if(sock.clientsock_num!=0)
		{
			DBG("main print :clientnum-%d\n",sock.clientsock_num);
			for(i=0;i<sock.clientsock_num;i++)
			{
				sock.clientsock_timeout[i]++;
				if(sock.clientsock_timeout[i]>30)//>1min
				{
					maintenancesockattr(sock.clientsock_attr[i],(void *)&sock,'r');
				}
				else
				{
					DBG("	CLIENT %d INFO : IP %s TIMEOUT %d\n",sock.clientsock_attr[i],sock.clientsock_ipattr[i],sock.clientsock_timeout[i]);
				}
			}
		}
	}
}

//timing request httpserver thread
void * thread_timingrequesthttp(void *arg)
{
	BYTE http_response[40]="";
	int http_responselen=0;
	int i,j;
	while(1)
	{
		char web_address[1024]="";
		memset( web_address, 0, sizeof(web_address) );
		strcat(web_address,HTTPWEBADDRONE);
		strcat(web_address,MAC);
		strcat(web_address,HTTPWEBADDRTWO);
		for( i=0;i<node_number;i++ )
		{
			unsigned char nodestring[41];
			memset( nodestring, 0, sizeof(nodestring) );
			for( j=0;j<20;j++ )
			{
				unsigned char tempstring[3]="";
				sprintf( tempstring,"%02x",node_value[i][j] );
				strcat( nodestring,tempstring );
				if(j>0)
				{
					if(( node_value[i][j]==0x0A )&&( node_value[i][j-1]==0x0D ))
					{
						break;
					}
				}
			}
			strcat(web_address,nodestring);
		}
		//MSG("request string : %s\n",web_address);
		http_responselen = gethttpstring(web_address,http_response);
		DBG( "\nFrom Httpserver Get %d Bytes : %s\n ", http_responselen/2-1,http_response );
		if( (http_responselen/2-1)>3 )//have commond to perform
		{
			BYTE http_commond[20]="";
			http_responselen = (http_responselen/2)-1;
			for( i=0;i<http_responselen;i++ )
			{
				http_commond[i] = ((atoh(http_response[i*2+1])<<4)&0xf0)|((atoh(http_response[i*2+2]))&0x0f);
				MSG("%02x ",http_commond[i]);
			}
			MSG("\n");
			if((http_commond[0]==0x24)&&(http_commond[http_responselen-2]==0x0D)&&(http_commond[http_responselen-1]==0x0A))
			{
				i = 0;
				if( http_commond[1] == 0x01 )//ң������ ���͵�ң��ģ��
				{
					if(( http_commond[2] == 0x03 )||( http_commond[2] == 0x04 ))
					{
						//���ȼ������ң��ģ��
						int remotecontrol_fd=0;
						for( i=0;i<sendtosocket->clientsock_num;i++ )
						{
							//���û�Ϊң��ģ�飬����ң������
							if( sendtosocket->clientsock_type[i] == 0x02 )
							{
								remotecontrol_fd = sendtosocket->clientsock_attr[i];
								perform_commond( remotecontrol_fd,http_commond,http_responselen );
								i=sendtosocket->clientsock_num;//==break;
							}
						}
					}
				}
				if( i==0 )//��ң������ ���͵�Ӳ���������Ľڵ�
				{
					sem_wait(&sendcontrolcommand_sem);//if sendcontrolcommand_sem == 0 then waitfor : -1
					write(serialhandle,http_commond,http_responselen);
					sem_post(&sendcontrolcommand_sem);//+1 
				}
			}
		}
		sleep( 1 );
	}
}

//timing gacquisition thread
void * thread_timingacquisition(void *arg)
{
	BYTE commandattr[9] = {0x24,0x00,0x00,0x00,0x01,0x00,0xFF,0x0D,0x0A};
	int i=0;
	//��ʱ�����ڵ�
	sem_post(&sendcontrolcommand_sem);//+1
	while(1)
	{
		for(i=0;i<node_number;i++)
		{
			commandattr[1]=node_id[i][0];
			commandattr[2]=node_id[i][1];
			commandattr[3]=node_id[i][2];
			commandattr[6]=commandattr[1]+commandattr[2]+commandattr[3]+commandattr[4]+commandattr[5];
			sem_wait(&sendcontrolcommand_sem);//if sendcontrolcommand_sem == 0 then waitfor -1
			write(serialhandle,commandattr,9);
			usleep(1000*15);//
			sem_post(&sendcontrolcommand_sem);//+1 
			////printf the serail come data
			/*int j=0;
			for(j=0;j<9;j++)
			{
			MSG("%02x ",commandattr[j]);
			}
			MSG("\n");*/
			if( 0x03 == commandattr[1] )//�ɼ��� ��ʱ��
			{
				usleep(1000*1000);//ÿ���ڵ�������Ϊ1000ms
			}
			else
			{
				usleep(1000*200);//ÿ���ڵ�������Ϊ200ms
			}
		}
		//sleep(1);
	}
}

//serial service thread
void * thread_serial(void *arg)
{
	struct termios comset;
	int serial_fd;
	fd_set readfds,testfds;//
	/*�Զ�д��ʽ�򿪴���*/
	serial_fd = open( arg, O_RDWR );
	if ( serial_fd <0 )
	{
		DBG("open serial err!!\n");
		pthread_exit("");
	}
	SetSpeed(serial_fd,BAUDRATE);
	if ( SetParity(serial_fd,8,1,'N') == FALSE )
	{
		DBG("Set Parity Error!!\n");
		pthread_exit("");
	}
	DBG("Open serial %d ok\n",serial_fd);
	/*select server socket*/
	serialhandle=serial_fd;
	FD_ZERO( &readfds );
	FD_SET( serial_fd,&readfds );
	int result,i,recvlen = 0;
	char recvbuf[64];
	char tempbuf[1024];
	while(1)
	{
		//DBG("serial server waiting...\n");
		testfds = readfds;
		result = select(serial_fd+1,&testfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0);
		if(result<1)
		{
			DBG("serial err!!\n");
			pthread_exit("");
		}
		else
		{
			//�Ӵ��ڻ�ȡ����
			result=read(serial_fd,recvbuf,sizeof(recvbuf));
			//printf the serail come data
			/*MSG("serial server recvbyte:");	
			for(i=0;i<result;i++)
			{
			MSG("%02x ",recvbuf[i]);
			}
			MSG("number:%d\n",result);*/
			//ת���������ݵ�����socket
			for( i=0;i<sendtosocket->clientsock_num;i++ )
			{
				if( sendtosocket->clientsock_type[i] == 0x01 )//����û�����
				{
					write(sendtosocket->clientsock_attr[i],recvbuf,result);
				}
			}
			//�洢�������ݵ��ڴ�
			for( i=0;i<result;i++ )
			{
				tempbuf[recvlen] = recvbuf[i];
				recvlen++;
				if(recvlen>1)
				{
					//�ж��Ƿ�Ϊ�������ݰ�
					if((tempbuf[recvlen-1] == 0x0A)&&(tempbuf[recvlen-2] == 0x0D))
					{
						int n = 0;
						for( n=0;n<node_number;n++ )
						{
							//�洢���ݵ���Ӧ�ڵ�
							if(( tempbuf[1] == node_value[n][1] )&&( tempbuf[2] == node_value[n][2] )&&( tempbuf[3] == node_value[n][3] ))
							{
								int j = 0;
								for( j=4;j<recvlen;j++ )
								{
									node_value[n][j] = tempbuf[j];
								}
								n = node_number;//==break
							}
						}
						recvlen = 0;//���յ��������ݰ�������ָ�룬������һ��
					}
				}
			}
		}
	}
}

//sock service thread
void * thread_socket(void *arg)
{
	pSOCK_ATTR sock=(pSOCK_ATTR)arg;//struct of diy_sock
	int server_sockfd;//server sock
	int server_len;//
	struct sockaddr_in server_addr;//
	int result;//function return value
	fd_set testfds;//

	/*init server socket*/
	server_sockfd=socket(AF_INET,SOCK_STREAM,0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons( (unsigned short) atoi( sock->s_port ) );
	server_len = sizeof(server_addr);
	/*bind server socket*/
	result = bind( server_sockfd, (struct sockaddr *)&server_addr, server_len );
	if( result == -1 )
	{
		//bind err
		DBG("bind socket err\n");
		pthread_exit("");
	}
	/*listen server socket*/
	result = listen( server_sockfd, BACKLOG );
	if( result == -1 )
	{
		//listen err
		DBG("listen socket err\n");
		pthread_exit("");
	}
	/*select server socket*/
	DBG("Open socket %d ok\n",server_sockfd);
	FD_ZERO( &sock->clientsock_fds );
	FD_SET( server_sockfd,&sock->clientsock_fds );
	int nread,fd,i,j;
	char recvbuf[64];
	while(1)
	{
		if(sock->clientsock_num==0)
			DBG("Socket server waiting for client...\n");
		testfds=sock->clientsock_fds;
		result = select(FD_SETSIZE,&testfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0);
		if(result<1)
		{
			DBG("Socket select err\n");
			pthread_exit("");
		}
		for(fd=0 ; fd<FD_SETSIZE;fd++)
		{
			if(FD_ISSET(fd,&testfds))
			{
				if(fd==server_sockfd)//��������Ϣ
				{
					//�����������
					maintenancesockattr(fd,(void*)sock,'a');
				}
				else//�ͻ�����Ϣ
				{
					ioctl(fd,FIONREAD,&nread);
					if(nread==0)//�ͻ������ӶϿ���Ϣ
					{
						maintenancesockattr(fd,(void*)sock,'r');
					}
					else//�ͻ���������Ϣ
					{
						int registeok=0;
						//���յ����ݣ�����fd��ʱ��
						//maintenancesockattr(fd,(void*)sock,'u');
						//ͨ��socket�������ݰ�
						result=read(fd,recvbuf,sizeof(recvbuf));
						//ע���û��������ݰ�
						if((recvbuf[0] == 'Z')&&(recvbuf[1] == 'C'))
						{
							//ע���û�
							for( i=0;i<sock->clientsock_num;i++ )
							{
								if( fd == sock->clientsock_attr[i] )
								{
									sock->clientsock_type[i] = recvbuf[2];
									//����ע��ɹ�
									recvbuf[3] = 'O';
									recvbuf[4] = 'K';
									write( fd,recvbuf,5 );
									i=sock->clientsock_num;//==break;
								}
							}
						}
						else//�жϸ��û��Ƿ��Ѿ�ע��
						{
							for( i=0;i<sock->clientsock_num;i++ )
							{
								if( fd == sock->clientsock_attr[i] )
								{
									if( sock->clientsock_type[i] != 0)//�Ѿ�ע��
									{
										registeok = 1;
									}
									i=sock->clientsock_num;//==break;
								}
							}
						}
						if( registeok == 1)//�û���ע��
						{
							//�Ž����ݰ�
							if((recvbuf[1]==0x02)&&(recvbuf[2]==0x03))
							{
								if( (recvbuf[5]+9) == result )
								{
									//ת���Ž����ݰ�������socket
									for( i=0;i<sendtosocket->clientsock_num;i++ )
									{
										if( sock->clientsock_type[i] == 0x01 )//����û�����
										{
											write(sendtosocket->clientsock_attr[i],recvbuf,result);
										}
									}
								}
							}
							//�������ݰ�
							else if((recvbuf[0]==0x0D)&&(recvbuf[1]==0x0A))
							{
								//���յ����ݣ�����fd��ʱ��
								maintenancesockattr(fd,(void*)sock,'u');
							}
							//�������ݰ�
							else
							{
								MSG("\nSocket server recvbyte:%d from fd:%d\n",result,fd);
								i = 0;
								if( recvbuf[1] == 0x01 )//ң������ ���͵�ң��ģ��
								{
									if(( recvbuf[2] == 0x03 )||( recvbuf[2] == 0x04 ))
									{
										//���ȼ������ң��ģ��
										int remotecontrol_fd=0;
										for( i=0;i<sock->clientsock_num;i++ )
										{
											//���û�Ϊң��ģ�飬����ң������
											if( sock->clientsock_type[i] == 0x02 )
											{
												remotecontrol_fd = sock->clientsock_attr[i];
												perform_commond( remotecontrol_fd,recvbuf,result );
												i=sock->clientsock_num;//==break;
											}
										}
									}
								}
								if( i==0 )//��ң������ ���͵�Ӳ���������Ľڵ�
								{
									sem_wait(&sendcontrolcommand_sem);//if sendcontrolcommand_sem == 0 then waitfor -1
									write(serialhandle,recvbuf,result);
									sem_post(&sendcontrolcommand_sem);//+1 
									MSG("\nSocket server send the recvbyte to serial ok\n");
								}
							}
						}
					}
				}
			}
		}
	}
}

//ִ��ң������
//@fd ���Ͷ˾��
//@commond_buf ��������
//@commond_len �����
static void perform_commond( int fd,char *commond_buf,int commond_len )
{
	int i=0;
	char commond_key[20];
	memset(commond_key,0,20);
	unsigned char commond_value[1024]="";
	memset(commond_value,0,1024);
	char temp[9]="";
	sprintf( temp,"%02x%02x%02x%02x",commond_buf[1],commond_buf[2],commond_buf[3],commond_buf[6] );
	strcat( commond_key,temp );
	commond_value[0] = commond_buf[2];
	commond_len=0;
	commond_len = GetIniKeyValueString( "REMOTECONTROLCOMMOND",commond_key,commond_value+1,NODECONFIG_INI_PATH );
	if( commond_len != 0 )//���ص�ң���������͵�ң��ģ��
	{
		//��ascall��תΪ16������
		commond_len = commond_len/2;
		for( i=0;i<commond_len;i++ )
		{
			commond_value[i+1] = ((atoh(commond_value[i*2+1])<<4)&0xf0)|((atoh(commond_value[i*2+2]))&0x0f);
		}
		commond_value[commond_len+1] = 0xFF;
		commond_value[commond_len+2] = 0xFF;
		commond_value[commond_len+3] = 0xFF;
		write( fd,commond_value,commond_len+4);
	}
	else
	{
		MSG("\nIn config.ini have on this commond : %s!\n",commond_key);
	}
}
