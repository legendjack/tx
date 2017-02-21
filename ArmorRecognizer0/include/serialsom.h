#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdint.h>
#define FALSE  -1
#define TRUE   0
using namespace std;

//@brief:
//@brief:linux下的串口通信类，可以通过构造函数直接打开一个串口，并初始化（默认9600波特率，8位数据，无奇偶校验，1位停止位）
//             send()成员函数可以直接发送字符串，set_opt()更改参数。串口会在析构函数中自动关闭
//@example:Serialport exp("/dev/ttyUSB0");
//                  exp.set_opt(115200,8,'N',1);
//                  exp.send("1123dd");
class Serialport
{
public:
        Serialport(char *port);//定义Serialport类的成员函数，

        ~ Serialport();
        int open_port(char *port); //
        int set_opt(int nSpeed = 115200 , int nBits = 8, char nEvent ='N', int nStop = 1);
        bool UART0_Send(uint8_t *str);
        bool UART0_Twist_Sent(short liner_x,short liner_y,short angular_z);
        int UART0_Recv(char *rcv_buf,int data_len);
        int UART0_Recv_Odm( int16_t *odm_buf);
        bool  UART0_INIT(void);
        bool usart3_send(uint8_t picth, uint8_t yaw);
private:
         int fd ;

         uint8_t tmp_sent[9];
         uint8_t *buffer;
         bool  serial_sucess;
         bool rec_fag;


};

//@brief:linux下的串口通信类的成员函数。
//       open_port()成员函数可以打开一个串口，set_opt()更改参数。
//@example:open_port("/dev/ttyUSB0");
//         set_opt(115200, 8, 'N', 1);
Serialport::Serialport(char *port)
{
        open_port(port);
        set_opt();

}

bool Serialport::UART0_INIT(void)
{

return serial_sucess;
}
//Serialport下的成员函数open_port()的实现；
int Serialport::open_port(  char*port)
{

    fd = open(port , O_RDWR|O_NOCTTY|O_NDELAY);
    if (-1 == fd)
    {
        perror("Can't Open Serial Port");
        serial_sucess=0;
    }
    else
    {
        fcntl(fd, F_SETFL, 0);
    }

    if(fcntl(fd, F_SETFL, 0)<0)
        printf("fcntl failed!\n");
    else
        printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));

    if(isatty(STDIN_FILENO)==0)
    {   printf("standard input is not a terminal device\n");
    serial_sucess=0;}
    else{
        printf("isatty success!\n");
         serial_sucess=1;
         }
    printf("fd-open=%d\n",fd);

    return fd;
}

    /*设置串口属性：
       fd: 文件描述符
       nSpeed: 波特率
       nBits: 数据位5
       nEvent: 奇偶校验
       nStop: 停止位*/
int Serialport::set_opt(int nSpeed , int nBits, char nEvent , int nStop )
{
    struct termios newtio,oldtio;
    if ( tcgetattr( fd,&oldtio) != 0)
    {
        perror("SetupSerial error");
        serial_sucess=0;
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;
    switch( nBits )
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }
    switch( nEvent )
    {
    case 'O':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E':
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N':
        newtio.c_cflag &= ~PARENB;
        break;
    }
    switch( nSpeed )
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }

    if( nStop == 1 )
        newtio.c_cflag &= ~CSTOPB;
    else if ( nStop == 2 )
        newtio.c_cflag |= CSTOPB;

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd,TCIFLUSH);

    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }
    printf("Serial port set done!\n");
    return 0;
}

bool Serialport::UART0_Send(uint8_t *str)
{
    buffer  = str;
    if(write(fd, buffer ,9)<0)
    {
        perror("write error");
        return false;
    }

    return true;
}
bool Serialport::UART0_Twist_Sent(short liner_x,short liner_y,short angular_z)
{ int a;

//        printf("1.sizeof(tmp_sent)=%d\n",sizeof(tmp_sent));
        tmp_sent[0]=0xA0;
        tmp_sent[1]=0x06;
        tmp_sent[2]=(liner_x>>8);
        tmp_sent[3]=(liner_x&0x00ff);
        tmp_sent[4]=(liner_y>>8);
        tmp_sent[5]=(liner_y&0x00ff);
        tmp_sent[6]=(angular_z>>8);
        tmp_sent[7]=(angular_z&0x00ff);
        tmp_sent[8]=0xA1;
//        printf("2.sizeof(tmp_sent)=%d\n",sizeof(tmp_sent));
        a=UART0_Send(tmp_sent);
        return a;

}
int Serialport::UART0_Recv(  char *rcv_buf,int data_len)
{
    int len,fs_sel;
    fd_set fs_read;

    struct timeval time;

    FD_ZERO(&fs_read);
    FD_SET(fd,&fs_read);

    time.tv_sec = 10;
    time.tv_usec = 0;

    //使用select实现串口的多路通信
    fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);
    if(fs_sel){
     len = read(fd,rcv_buf,data_len);
     return len;
        } else {
        return FALSE;
    }
}
int Serialport::UART0_Recv_Odm( int16_t *odm_buf)
{
    int len,fs_sel;
    bool a,b,c,d;
    fd_set fs_read;
    uint8_t rcv_buf[38];//注意char范围-128~127.unsigned char 0~255.字节没有符号位之说,表示字节都用unsigned char

    struct timeval time;

    FD_ZERO(&fs_read);//每次都要清空集合，否则布恩那个检测描述符变化
    FD_SET(fd,&fs_read);//添加描述符

    time.tv_sec = 0;//设置等待时间s
    time.tv_usec = 0;
    //使用select实现串口的多路通信
// printf("Serialport : come in serial\n");
    fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);
    if(fs_sel)//只有接收到串口数据才会进入到这里,如果想要每次进入都接收到,就要保证接受的频率和发送的频率一致
       {

   //   printf("Serialport : ready to receive odom\n");
          len = read(fd,rcv_buf,38);//放38个字节进来，最少能够进入1个19位字节的odm信息
          for(int i=0;i<18;i++)
          {
           a=(rcv_buf[i]==0xb0);
           b=(rcv_buf[i+1]==0x10);
           c=(rcv_buf[i+18]==0xb1);
           d=a&&b&&c;
           if(d)
            {
              for(int j=0;j<8;j++)
                  {
                    odm_buf[j]=rcv_buf[i+2*(j+1)]*256+rcv_buf[i+2*(j+1)+1];
               printf("Serialport : odm_buf[%d]=%d\n",j, odm_buf[j]);
                   }
            }
           break;
          }
          return len;
       }
    else return FALSE;
}


bool Serialport::usart3_send(uint8_t picth, uint8_t yaw)
{
  int a;
  uint8_t data_temp[7];
  data_temp[0] = 0xaa;
  data_temp[1] = 0xab;
  data_temp[2] = picth>>8;
  data_temp[3] = picth&0xff;
  data_temp[4] = yaw>>8;
  data_temp[5] = yaw&0xff;
  data_temp[6] = 0xac;

  a = UART0_Send(data_temp);
  return a;
}

Serialport:: ~ Serialport()
{
    close(fd);
}

