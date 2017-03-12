#include <sys/types.h>
#include <stdio.h>/*标准输入输出定义*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>/*Unix 标准函数定义*/
#include <stdlib.h>/*标准函数库定义*/
#include <pthread.h>
#include <termios.h>/*PPSIX 终端控制定义*/
#include <fcntl.h>/*文件控制定义*/
#include <arpa/inet.h>/*inet_ntoa*/
#include <string.h>/*strcpy*/
#include <semaphore.h>//sem 
#include "debug.h"
#include "serial.h"
#include "operateini.h"
#include "httpclient.h"

/* baudrate settings are defined in <asm/termbits.h>, which is included by <termios.h> */
#define BAUDRATE 9600

//节点信息配置文件路径
#define NODECONFIG_INI_PATH "./config.ini"

//最大节点数
#define NODEMAXNUMBER 100

//http网络地址
#define HTTPWEBADDRONE "http://115.24.161.178:8080/SmartHomeBack/ajax/FacilitySignalAction.action?code="
#define HTTPWEBADDRTWO "&signal="
#define HTTPWEBADDRTEST "http://115.24.161.158:8080/DCPDispatcher/ajax/DispatchHeartPhoneAction.action"

//本机MAC地址
#define MAC "ff000001"

//
#define SOCKIPLEN 16
#define BACKLOG 10
#define TRUE 1
#define FALSE 0
#define BYTE unsigned char

typedef struct _sock_attr{
	char *s_port;					//监听端口号
	fd_set clientsock_fds;
	int clientsock_attr[FD_SETSIZE];		//存放socket句柄
	int clientsock_num;				//客户端链接数
	int clientsock_timeout[FD_SETSIZE];//客户端socket超时数
	char clientsock_ipattr[FD_SETSIZE][SOCKIPLEN];	//客户端ip地址
	unsigned char clientsock_type[FD_SETSIZE];//客户端类型0：非法用户 1：前台显示界面 2：遥控模块 3：门禁模块
}*pSOCK_ATTR,SOCK_ATTR;

//函数声明
void * thread_socket(void *arg);
void * thread_serial(void *arg);
void * thread_timingacquisition(void *arg);
void * thread_timingrequesthttp(void *arg);
static void perform_commond( int fd,char *commond_buf,int commond_len );

#define NODEIDATTRLEN 3
#define NODEVALUEATTRLEN 20
//变量声明
static pSOCK_ATTR sendtosocket;
static int serialhandle;
static BYTE node_id[NODEMAXNUMBER][NODEIDATTRLEN] = { 0 }; 
static BYTE node_value[NODEMAXNUMBER][NODEVALUEATTRLEN] = { 0 };
static int node_number = 0;
static sem_t sendcontrolcommand_sem;


/**
@maintenancesockattr : 维护客户端sock队列函数
@
@int fd : 需关闭的sock句柄
@void* arg : 需维护的sock队列指针
@char type : 操作类型 'a'==add 'r'==remove 'u'==update
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
		//获得客户端连接socket
		client_sockfd=accept(fd,(struct sockaddr *)&client_addr,&client_len);
		if(client_sockfd==-1)
		{
			DBG("Accept err\n");
		}
		else
		{
			//加入监听队列
			FD_SET(client_sockfd,&sock->clientsock_fds);
			sock->clientsock_attr[sock->clientsock_num]=client_sockfd;//存储客户端sock
			sock->clientsock_timeout[sock->clientsock_num]=0;//初始化客户端超时数
			sock->clientsock_type[sock->clientsock_num] = 0;//初始化客户端连接类型
			ip=inet_ntoa(client_addr.sin_addr);
			memset(sock->clientsock_ipattr[sock->clientsock_num],0,16);
			strcpy(sock->clientsock_ipattr[sock->clientsock_num],ip);//存储客户端ip
			//DBG("Adding client on fd %d\n",client_sockfd);
			sock->clientsock_num++;
			//DBG("Current client numbers : %d\n",sock->clientsock_num);
		}
	}
}

/**
@initdbglog : 初始化调试日志
@
@char* path : 日志存放路径
**/
void initdbglog( char* path )
{
	debug_term_on();
	debug_set_dir( path );
	debug_file_on();
}

//由ascll码转十六进制 
//如 'a'->0x0a '1'->0x01
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
@loadnodeconf : 导入节点信息
@id ：十六进制方式存储配置文件内节点ID
@value：字符串方式存储配置文件内节点ID
@number：存储节点数量
**/
static int loadnodeconf( BYTE (*id)[NODEIDATTRLEN],BYTE (*value)[NODEVALUEATTRLEN],int *number )
{
	BYTE tmpnodeid[NODEMAXNUMBER*10] = { 0 }; 
	//从文件导入节点ID
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
@main : main函数
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
				if( http_commond[1] == 0x01 )//遥控命令 发送到遥控模块
				{
					if(( http_commond[2] == 0x03 )||( http_commond[2] == 0x04 ))
					{
						//优先检查有无遥控模块
						int remotecontrol_fd=0;
						for( i=0;i<sendtosocket->clientsock_num;i++ )
						{
							//若用户为遥控模块，则发送遥控命令
							if( sendtosocket->clientsock_type[i] == 0x02 )
							{
								remotecontrol_fd = sendtosocket->clientsock_attr[i];
								perform_commond( remotecontrol_fd,http_commond,http_responselen );
								i=sendtosocket->clientsock_num;//==break;
							}
						}
					}
				}
				if( i==0 )//非遥控命令 发送到硬件网络中心节点
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
	//定时遍历节点
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
			if( 0x03 == commandattr[1] )//采集类 耗时长
			{
				usleep(1000*1000);//每个节点遍历间隔为1000ms
			}
			else
			{
				usleep(1000*200);//每个节点遍历间隔为200ms
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
	/*以读写方式打开串口*/
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
			//从串口获取数据
			result=read(serial_fd,recvbuf,sizeof(recvbuf));
			//printf the serail come data
			/*MSG("serial server recvbyte:");	
			for(i=0;i<result;i++)
			{
			MSG("%02x ",recvbuf[i]);
			}
			MSG("number:%d\n",result);*/
			//转发串口数据到界面socket
			for( i=0;i<sendtosocket->clientsock_num;i++ )
			{
				if( sendtosocket->clientsock_type[i] == 0x01 )//检查用户类型
				{
					write(sendtosocket->clientsock_attr[i],recvbuf,result);
				}
			}
			//存储串口数据到内存
			for( i=0;i<result;i++ )
			{
				tempbuf[recvlen] = recvbuf[i];
				recvlen++;
				if(recvlen>1)
				{
					//判断是否为完整数据包
					if((tempbuf[recvlen-1] == 0x0A)&&(tempbuf[recvlen-2] == 0x0D))
					{
						int n = 0;
						for( n=0;n<node_number;n++ )
						{
							//存储数据到相应节点
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
						recvlen = 0;//接收到完整数据包，重置指针，接收下一包
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
				if(fd==server_sockfd)//服务器消息
				{
					//加入监听队列
					maintenancesockattr(fd,(void*)sock,'a');
				}
				else//客户端消息
				{
					ioctl(fd,FIONREAD,&nread);
					if(nread==0)//客户端链接断开消息
					{
						maintenancesockattr(fd,(void*)sock,'r');
					}
					else//客户端其他消息
					{
						int registeok=0;
						//接收到数据，更新fd超时数
						//maintenancesockattr(fd,(void*)sock,'u');
						//通过socket接收数据包
						result=read(fd,recvbuf,sizeof(recvbuf));
						//注册用户类型数据包
						if((recvbuf[0] == 'Z')&&(recvbuf[1] == 'C'))
						{
							//注册用户
							for( i=0;i<sock->clientsock_num;i++ )
							{
								if( fd == sock->clientsock_attr[i] )
								{
									sock->clientsock_type[i] = recvbuf[2];
									//返回注册成功
									recvbuf[3] = 'O';
									recvbuf[4] = 'K';
									write( fd,recvbuf,5 );
									i=sock->clientsock_num;//==break;
								}
							}
						}
						else//判断该用户是否已经注册
						{
							for( i=0;i<sock->clientsock_num;i++ )
							{
								if( fd == sock->clientsock_attr[i] )
								{
									if( sock->clientsock_type[i] != 0)//已经注册
									{
										registeok = 1;
									}
									i=sock->clientsock_num;//==break;
								}
							}
						}
						if( registeok == 1)//用户已注册
						{
							//门禁数据包
							if((recvbuf[1]==0x02)&&(recvbuf[2]==0x03))
							{
								if( (recvbuf[5]+9) == result )
								{
									//转发门禁数据包到界面socket
									for( i=0;i<sendtosocket->clientsock_num;i++ )
									{
										if( sock->clientsock_type[i] == 0x01 )//检查用户类型
										{
											write(sendtosocket->clientsock_attr[i],recvbuf,result);
										}
									}
								}
							}
							//心跳数据包
							else if((recvbuf[0]==0x0D)&&(recvbuf[1]==0x0A))
							{
								//接收到数据，更新fd超时数
								maintenancesockattr(fd,(void*)sock,'u');
							}
							//命令数据包
							else
							{
								MSG("\nSocket server recvbyte:%d from fd:%d\n",result,fd);
								i = 0;
								if( recvbuf[1] == 0x01 )//遥控命令 发送到遥控模块
								{
									if(( recvbuf[2] == 0x03 )||( recvbuf[2] == 0x04 ))
									{
										//优先检查有无遥控模块
										int remotecontrol_fd=0;
										for( i=0;i<sock->clientsock_num;i++ )
										{
											//若用户为遥控模块，则发送遥控命令
											if( sock->clientsock_type[i] == 0x02 )
											{
												remotecontrol_fd = sock->clientsock_attr[i];
												perform_commond( remotecontrol_fd,recvbuf,result );
												i=sock->clientsock_num;//==break;
											}
										}
									}
								}
								if( i==0 )//非遥控命令 发送到硬件网络中心节点
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

//执行遥控命令
//@fd 发送端句柄
//@commond_buf 发送命令
//@commond_len 命令长度
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
	if( commond_len != 0 )//加载到遥控命令则发送到遥控模块
	{
		//将ascall码转为16进制数
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
