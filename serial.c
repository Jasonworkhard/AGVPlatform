#include <termios.h>
#include <stdio.h>
#include "serial.h"

static int speed_arr[] = { B38400, B19200, B115200, B9600, B4800, B2400, B1200, B300, B38400, B19200, B9600, B4800, B2400, B1200, B300, };
static int name_arr[] = {38400, 19200, 115200, 9600, 4800, 2400, 1200, 300, 38400, 19200, 9600, 4800, 2400, 1200, 300, };

/**
*@brief  设置串口通信速率
*@param  fd     类型 int  打开串口的文件句柄
*@param  speed  类型 int  串口速度
*@return  void
*/
void SetSpeed(int fd, int speed)
{
	int i;
	struct termios Opt;    //定义termios结构
	if(tcgetattr(fd, &Opt) != 0)
	{
		perror("tcgetattr fd");
		return;
	}
	for(i = 0; i < sizeof(speed_arr) / sizeof(int); i++)
	{
		if(speed == name_arr[i])
		{
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]);
			cfsetospeed(&Opt, speed_arr[i]);
			if(tcsetattr(fd, TCSANOW, &Opt) != 0)
			{
				perror("tcsetattr fd");
				return;
			}
			tcflush(fd, TCIOFLUSH);
			return;
		}
	}
}

/**
*@brief   设置串口数据位，停止位和效验位
*@param  fd     类型  int  打开的串口文件句柄*
*@param  databits 类型  int 数据位   取值 为 7 或者8*
*@param  stopbits 类型  int 停止位   取值为 1 或者2*
*@param  parity  类型  int  效验类型 取值为N,E,O,,S
*/
int SetParity(int fd, int databits, int stopbits, int parity)
{
	struct termios Opt;
	if(tcgetattr(fd, &Opt) != 0)
	{
		perror("tcgetattr fd");
		return 0;
	}

	switch(databits)        //设置数据位数
	{
	case 7:
		Opt.c_cflag &= ~CSIZE;
		Opt.c_cflag |= CS7;
		break;
	case 8:
		Opt.c_cflag &= ~CSIZE;
		Opt.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Unsupported data size.\n");
		return 0;
	}

	switch(parity)            //设置校验位
	{
	case 'n':
	case 'N':
		Opt.c_cflag &= ~PARENB;       //清除校验位
		Opt.c_iflag &= ~INPCK;        //disable parity checking
		break;
	case 'o':
	case 'O':
		Opt.c_cflag |= PARENB;        //enable parity 允许校验位
		Opt.c_cflag |= PARODD;        //奇校验
		Opt.c_iflag |= INPCK;         //Enable parity check 
		break;
	case 'e':
	case 'E':
		Opt.c_cflag |= PARENB;        //enable parity 允许校验位
		Opt.c_cflag &= ~PARODD;       //偶校验
		Opt.c_iflag |= INPCK;         //Enable parity check 
		break;
	case 's':
	case 'S':
		Opt.c_cflag &= ~PARENB;        //清除校验位
		Opt.c_cflag &= ~CSTOPB;        //2个停止位(清除该标志表示1个停止位)
		Opt.c_iflag |= INPCK;          //Enable parity check 
		break;
	default:
		fprintf(stderr, "Unsupported parity.\n");
		return 0;    
	}

	switch(stopbits)        //设置停止位
	{
	case 1:
		Opt.c_cflag &= ~CSTOPB;		//2个停止位(清除该标志表示1个停止位)
		break;
	case 2:
		Opt.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "Unsupported stopbits.\n");
		return 0;
	}

	Opt.c_cflag |= (CLOCAL | CREAD);	//一般必设置的标志  
	//Local line – do not change “owner” of port  |  Enable receiver

	Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	//ICANON:启用标准模式 (canonical mode)。允许使用特殊字符 EOF, EOL, EOL2, ERASE, KILL, LNEXT, REPRINT, STATUS, 和 WERASE，以及按行的缓冲。
	//ECHO:回显输入字符。
	//ECHOE:如果同时设置了 ICANON，字符 ERASE 擦除前一个输入字符，WERASE 擦除前一个词。
	//ISIG:当接受到字符 INTR, QUIT, SUSP, 或 DSUSP 时，产生相应的信号。

	Opt.c_oflag &= ~OPOST;
	//OPOST:启用具体实现自行定义的输出处理。
	Opt.c_oflag &= ~(ONLCR | OCRNL);
	//ONLCR:(XSI) 将输出中的新行符映射为回车-换行。
	//OCRNL:将输出中的回车映射为新行符

	Opt.c_iflag &= ~(ICRNL | INLCR);
	//ICRNL:将输入中的回车翻译为新行 (除非设置了 IGNCR)。
	//INLCR:将输入中的 NL 翻译为 CR。
	Opt.c_iflag &= ~(IXON | IXOFF | IXANY);
	//IXON:启用输出的 XON/XOFF流控制。
	//IXOFF:启用输入的 XON/XOFF流控制。
	//IXANY:(不属于 POSIX.1；XSI) 允许任何字符来重新开始输出。(?)

	tcflush(fd, TCIFLUSH);
	Opt.c_cc[VTIME] = 150;        //设置超时为15sec
	Opt.c_cc[VMIN] = 0;        //Update the Opt and do it now
	if(tcsetattr(fd, TCSANOW, &Opt) != 0)
	{
		perror("tcsetattr fd");
		return 0;
	}

	return 1;
}
