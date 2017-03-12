
#include <string.h> 
#include <stdio.h>
#include "operateini.h"

//��ȡKey
int GetKeyString( char *title,char *filename,unsigned char *nodeid )
{
	FILE *fp=NULL;
	if((fp = fopen(filename, "r")) == NULL)   
	{   
		printf(" Have no NODECONFIG_INI file!!\n");  
		return 0;
	}  
	char szLine[1024]="";
	char tmpstr[1024]="";
	memset( szLine,0,1024 );
	memset( tmpstr,0,1024 );
	strcpy(tmpstr,"[");
	strcat(tmpstr,title);
	strcat(tmpstr,"]");
	int rtnval=0;
	int i = 0;
	int flag = 0;
	int nodenumber=0;
	while(!feof(fp))
	{
		rtnval = fgetc(fp);
		if(rtnval == EOF)
		{
			break;
		}
		else   
		{
			if( i==1023 )
			{
				i = 0;
			}
			szLine[i] = rtnval;
			i++;
		}
		if(rtnval == '\n')
		{
			if( i>0 )
			{
				i--;
				szLine[i] = '\0'; //��\nתΪ\0
			}
			if( szLine[0] == '[' )//��һ�ο�ʼ
			{
				if( flag == 1)//key�ν���
				{
					flag = 0;
					break;
				}
				if( strncmp(tmpstr,szLine,strlen(tmpstr)) == 0 )
				{
					//�ҵ�title
					printf(" Find the title [%s]\n",title); 
					flag = 1;
				}
			}
			if( flag == 1 )
			{
				//ע����   
				if ('#' == szLine[0])
				{
					//printf(" %s\n ",szLine);
				}    
				else  
				{
					if( szLine[0] == '0' )
					{
						szLine[6]=0;
						printf(" NODEID : %s\n",szLine);
						//�Ҵ�key��Ӧ����  
						int j=0;
						for( j=0;j<6;j++ )
						{
							nodeid[nodenumber*10+j]=szLine[j];
						}
						nodenumber++;
					}
				}

			}  
			i = 0;
			memset( szLine,0,strlen(szLine) );
		}
	}
	fclose(fp);
	return nodenumber;
}


//��INI�ļ���ȡ�ַ����������� 
int GetIniKeyValueString(char *title,char *key,char *tmpstr,char *filename)
{  
	FILE *fp;
	char szLine[2048];
	memset( szLine,0,2048 );
	strcpy(tmpstr,"[");
	strcat(tmpstr,title);
	strcat(tmpstr,"]");
	int rtnval;
	int i = 0;
	int flag = 0;
	char *tmp;
	if((fp = fopen(filename, "r")) == NULL)
	{  
		printf(" Have no NODECONFIG_INI file!!\n");
		return 0;
	}
	while(!feof(fp))
	{
		rtnval = fgetc(fp);
		if( rtnval == EOF )
		{
			break;
		}
		else
		{
			if( rtnval != 0x20 )
			{
				if( i==2047 )
				{
					i = 0;
				}
				//printf("%c",rtnval);
				szLine[i] = rtnval;
				i++;
			}
		}
		if( rtnval == '\n' )
		{
			if( i>0 )
			{
				i--;
				szLine[i] = '\0'; //��\nתΪ\0
			}
			if( szLine[0] == '[' )//��ʼ�¶�
			{
				if( flag == 1 )//title���������
				{
					break;
				}
				if( strncmp(tmpstr,szLine,strlen(tmpstr)) == 0 )
				{
					//�ҵ�title
					printf(" Find the title [%s]\n",title);
					flag = 1;
				}
			}
			if( flag == 1 )
			{
				printf(" %s\n",szLine);
				tmp = strchr(szLine, '=');
				if( tmp != NULL )
				{  
					if( strstr(szLine,key)!=NULL )  
					{  
						//�ҵ�key��Ӧ����
						strcpy(tmpstr,tmp+1);
						fclose(fp);
						return strlen(tmpstr); 
					}  
				} 
			}
			i = 0;
			memset( szLine,0,strlen(szLine) );
		}
	}
	fclose(fp);
	return 0;
} 

