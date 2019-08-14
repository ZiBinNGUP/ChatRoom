#ifndef LINKER_H
#define LINKER_H

/* 服务器端口信息 */
#define PORTLINK ".chatport"

/* 缓存限制 */
#define MAXNAMELEN 256
#define MAXPKTLEN 2048

/* 信息类型的定义 */
#define LIST_GROUPS 0
#define JOIN_GROUP  1
#define LEAVE_GROUP 2
#define USER_TEXT   3
#define JOIN_REJECTED 4
#define JOIN_ACCEPTED 5

/* 数据包结构 */
typedef struct _packet
{
    /* 数据包类型 */
    char type;

    /* 数据包内容长度 */
    long  lent;

    //数据包内容
    char * text;
}Packet;

/* 开启服务器网络链接，使服务器处于监听状态 */
extern int startserver();
// extern int hooktoserver();
extern int locateserver();

/* 接受数据包 */
extern Packet *recvpkt(int sd);

/* 发送数据包 */
extern int sendpkt(int sd, char typ, long len, char *buf);

/* 释放数据包占用的内存 */
extern void freepkt(Packet *msg);


#endif