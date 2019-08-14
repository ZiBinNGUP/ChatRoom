/* 连接服务器和客户机的函数 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include "Linker.h"
#define DEBUG 0
int startserver()
{
    int sd;  // socket 描述符
    int myport; // 服务器端口
    const char * myname; // 本地主机名
    char linktrgt[MAXNAMELEN];
    char linkname[MAXNAMELEN];
    struct sockaddr_in server_addr; // 服务器套接字地址结构

    /* 调用socket函数创建 TCP socket 描述符 */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* 初始化服务器套接字地址 */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(0);

    /* 调用bind函数将一个本地地址指派给 socket */
    bind(sd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));

    /* 调用listen 将服务器端 socket 描述符 sd设置为被动地监听状态，并设置接受队列的长度为20 */
    listen(sd, 20);

    /* 调用 getsockname、gethostname、gethostbyname
        确定本地主机名和服务器端口号 */
    char hostname[MAXNAMELEN];
    if(gethostname(hostname, sizeof hostname) != 0)
        perror("gethostname");
    struct hostent *h;
    h = gethostbyname(hostname);

    int len = sizeof(struct sockaddr);
    getsockname(sd, (struct sockaddr *) &server_addr, &len);
    myname = h->h_name;
    myport = ntohs(server_addr.sin_port);

    /* 在家目录下创建符号链接‘.chatport’指向linktrgt */
    sprintf(linktrgt, "%s:%d", myname, myport);
    sprintf(linkname, "%s/%s", getenv("HOME"), PORTLINK);

    if(symlink(linktrgt, linkname) != 0)
    {
        fprintf(stderr, "error : server already exists\n");
        return(-1);
    }

    /* 准备接受客户端请求 */
    printf("admin: start server on '%s' at '%d'\n",
        myname, myport);
    return sd;
}

/* 和服务器建立连接， 正确返回socket描述符 失败返回-1 */
int hooktoserver()
{
    int sd;                           //套接字描述符

    char linkname[MAXNAMELEN];        //链接名
    char linktrgt[MAXNAMELEN];        //链接目标
    char *servhost;                   //服务器地址
    char *servport;                   //服务器端口号
    int bytecnt;
    
    /* 获取服务器地址 */
    sprintf(linkname, "%s/%s", getenv("HOME"), PORTLINK);
    bytecnt = readlink(linkname, linktrgt, MAXNAMELEN);
    if (bytecnt == -1)
    {
        fprintf(stderr, "error : no active chat server\n");
        return (-1);
    }
    
    linktrgt[bytecnt] = '\0';

    /* 获取服务器IP地址和端口号 */
    servport = index(linktrgt, ':');
    *servport = '\0';
    servport++;
    servhost = linktrgt;

    /* 获取服务器IP地址的unsigned short形式 */
    unsigned short number = (unsigned short) strtoul(servport, NULL, 0);

    /* 调用函数socket创建TCP套接字 */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* 调用 gethostbyname() 和 connect()连接’servhost‘ 的 ’servport‘端口 */
    struct hostent *hostinfo;
    struct sockaddr_in address;
    hostinfo = gethostbyname(servhost);/* 得到服务器主机名 */
    address.sin_family = AF_INET;
    address.sin_addr = *(struct in_addr *)*hostinfo->h_addr_list;
    address.sin_port = htons(number);

    if(connect(sd, (struct sockaddr*)&address, sizeof(address)))
    {
        perror("connecting");
        exit(1);
    }

    /* 连接成功 */
    printf("admin: connected to server on '%s' at '%s'\n",
        servhost, servport);
    return sd;

}

/* 从内核中读取一个套接字信息 */
int readn(int sd, char *buf, int n)
{
    int toberead;
    char* ptr;

    toberead = n;
    ptr = buf;
    while(toberead > 0)
    {
        int byteread;
        byteread = read(sd, ptr, toberead);
        if(byteread <= 0)
        {
            if(byteread == -1)perror("read");
            return 0;
        }
        toberead -= byteread;
        ptr += byteread;
    }
    return 1;
}

/* 接收数据包 */
Packet *recvpkt(int sd)
{
    Packet *pkt;
    /* 动态分配内存 */
    pkt = (Packet*)calloc(1, sizeof(Packet));
    if(!pkt){
        fprintf(stderr, "error : unable to calloc\n");
        return NULL;

    }
    int rn;
    /* 读取消息类型 */
    if((rn = readn(sd, (char*)&pkt->type, sizeof(pkt->type))) == 0)
    {
        free(pkt);
        return NULL;
    }
    #if DEBUG
        printf("DEBUG %s:type = %d rn = %d\n",__func__, pkt->type, rn);
    #endif
    

    /* 读取消息长度 */
    if((rn = readn(sd, (char *)&pkt->lent, sizeof(pkt->lent))) == 0)
    {
        free(pkt);
        return(NULL);
    }
    pkt->lent = ntohl(pkt->lent);
    #if DEBUG
        printf("DEBUG %s:lent = %d rn = %d\n",__func__, pkt->lent, rn);
    #endif
    /* 为消息内容分配空间 */
    if(pkt->lent > 0)
    {
        pkt->text = (char *)malloc(pkt->lent);
        if(!pkt->text)
        {
            fprintf(stderr, "error : unable to malloc\n");
            return NULL;
        }
    }

    if(!readn(sd, pkt->text, pkt->lent))
    {
        freepkt(pkt);
        return NULL;
    }
    return (pkt);
}

/* 发送数据包 */
int sendpkt(int sd, char typ, long len, char *buf)
{
    char tmp[8];
    long siz;
    /* 把包的类型和长度写入套接字 */
    bcopy(&typ, tmp, sizeof(typ));
    siz = htonl(len);
    bcopy((char *)&siz, tmp+sizeof(typ), sizeof(len));

    write(sd, tmp, sizeof(typ) + sizeof(len));

    if(len > 0)
        write(sd, buf, len);
    return 1;

}

/* 释放数据包占用的内存空间 */
void freepkt(Packet *pkt)
{
    free(pkt->text);
    free(pkt);
}