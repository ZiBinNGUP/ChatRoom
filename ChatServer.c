/* 聊天室服务端程序 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "Linker.h"
#define DEBUG 0
/* 聊天室成员信息 */
typedef struct _member
{
    /* 成员姓名 */
    char *name;

    /* 成员 socket 描述符*/
    int sock;

    /* 成员所属聊天室 */
    int grid;

    /* 下一个成员 */
    struct _member *next;

    /* 前一个成员 */
    struct _member *prev;
}Member;
/* 聊天室信息 */
typedef struct _group
{
    /* 聊天室名字 */
    char *name;

    /* 聊天室最大容量（人数） */
    int capa;

    /* 当前人数 */
    int occu;

    /* 记录聊天室内所有成员信息的链表 */
    Member* mems;
}Group;

/* 所有聊天室的信息表 */
Group *group;
int ngroups;

/* 通过聊天室名字找到聊天室ID */
int FindGroup(char *name)
{
    int grid;
    for(grid = 0; grid < ngroups; grid++)
    {
        if(strcmp(group[grid].name, name) == 0)
            return grid;
    }
    return -1;
}

/* 通过室成员名字找到室成员信息 */
Member *FindMemberByName(char* name)
{
    int grid;
    for(grid = 0; grid < ngroups; grid++)
    {
        Member *memb;
        /*遍历该组的所有成员*/
        for(memb = group[grid].mems; memb; memb = memb->next)
        {
            if(strcmp(memb->name, name) == 0)
            return memb;
        }
    }
    return NULL;
}

/* 通过socket描述符找到室成员的信息 */
Member *FindMemberBySock(int sock)
{
    int grid; /* 聊天室ID */

    /* 遍历所有的聊天室 */
    for (grid=0; grid < ngroups; grid++) 
	{
		Member *memb;

		/* 遍历所有的当前聊天室成员 */
		for (memb = group[grid].mems; memb; memb = memb->next)
		{
			if (memb->sock == sock)
			return(memb);
		}
    }
    return(NULL);
}

void cleanup()
{
    char linkname[MAXNAMELEN];

    /* 取消文件连接 */
    sprintf(linkname, "%s/%s",getenv("HOME"), PORTLINK);
    unlink(linkname);
    exit(0);
}

void main(int argc, char **argv)
{
    int servsock; //聊天室服务器端监听 socket 描述符
    int maxsd;      // 连接的客户端socket描述符的最大值
    fd_set livesdset, tempset; //客户端sockets描述符集

    /* 用户输入合法性检测 */
    if(argc != 2)
    {
        fprintf(stderr, "usage : %s <groups-file>\n",*argv);
        exit(1);
    }
    /* 调用 initgroups 函数， 初始化聊天室信息 */
    if(!initgroups(argv[1]))
        exit(1);
    
    /* 设置信号处理函数 */
    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);
    signal(SIGSEGV, cleanup);
    /* 准备接受请求 */
    servsock = startserver();

    if(servsock == -1)
    {
        exit(1);
    }

    /* 初始化 maxsd */
    maxsd = servsock;

    /* 初始化描述符集 */
    FD_ZERO(&livesdset);
    FD_ZERO(&tempset);
    FD_SET(servsock, &livesdset);

    while(1)
    {
        int sock;

        tempset = livesdset;

        select(maxsd + 1, &tempset, NULL, NULL, NULL);
        for(sock = 3; sock <= maxsd; sock ++)
        {
            if(sock == servsock)
                continue;

            if(FD_ISSET(sock, &tempset))
            {
                Packet *pkt;
                pkt = recvpkt(sock);
                if(!pkt)
                {
                    char *clientname;

                    socklen_t len;
                    struct sockaddr_in addr;
                    len = sizeof(addr);
                    if(getpeername(sock, (struct sockaddr*) &addr, &len) == 0)
                    {
                        struct sockaddr_in *s = (struct sockaddr_in *) &addr;
                        struct hostent *he;
                        he = gethostbyaddr(&s->sin_addr, sizeof(struct in_addr), AF_INET);
                        clientname = he->h_name;
                    }
                    else printf("Cannot get peer name");
                    printf("admin : disconnect from '%s' at '%d'\n",
                            clientname, sock);
                    
                    /* 从聊天室删除该成员 */
                    leavegroup(sock);

                    /* 关闭套接字 */
                    close(sock);

                    /* 清除套接字描述符在livesdset中的比特位 */
                    FD_CLR(sock, &livesdset);
                }
                else
                {
                    char *gname, *mname;
                    #if DEBUG
                    printf("tag 01\n");
                    #endif
                    /* 基于消息类型采取行动 */
                    switch(pkt->type)
                    {
                        case LIST_GROUPS :
                            #if DEBUG
                            printf("tag 02\n");
                            #endif
                            listgroups(sock);
                            break;
                        case JOIN_GROUP :
                            gname = pkt->text;
                            mname = gname + strlen(gname) + 1;
                            joingroup(sock, gname, mname);
                            break;
                        case LEAVE_GROUP :
                            leavegroup(sock);
                            break;
                        case USER_TEXT :
                            relaymsg(sock, pkt->text);
                            break;
                    }
                    /* 释放包结构 */
                    freepkt(pkt);
                }
                
            }
        }
        struct sockaddr_in remoteaddr;
        socklen_t addrlen;
        /* 有来自新的客户机的连接请求 */
        if(FD_ISSET(servsock, &tempset))
        {
            int csd;
            addrlen = sizeof remoteaddr;
            csd = accept(servsock, (struct sockaddr*)&remoteaddr, &addrlen);

            if(csd != 1)
            {
                char *clientname;
                struct hostent *h;
                h = gethostbyaddr((char *)&remoteaddr.sin_addr.s_addr, sizeof(struct in_addr), AF_INET);
                if(h != (struct hostent *)0)
                    clientname = h->h_name;
                else
                {
                    printf("gethostbyaddr failed\n");
                }
                /* 显示客户机的主机名和对应的 socket 描述符 */
				printf("admin: connect from '%s' at '%d'\n",
					clientname, csd);

				/* 将该连接的套接字描述符 csd 加入livesdset */
				FD_SET(csd, &livesdset);

				/* 保持 maxsd 记录的是最大的套接字描述符 */
				if (csd > maxsd)
					maxsd = csd;
                
            }
            else
            {
                perror("accept");
                exit(0);
            }
            
        }
    }

    
}
int initgroups(char* groupsFile)
{
    FILE *fp;
    char name[MAXNAMELEN];
    int capa;
    int grid;

    /* 打开存储聊天室信息的配置文件 */
    fp = fopen(groupsFile, "r");
    if(!fp)
    {
        fprintf(stderr, "error : ubable to open file ");
        return 0;
    }

    /* 从配置文件中读取聊天室的数量 */
    fscanf(fp, "%d", &ngroups);

    group = (Group*)calloc(ngroups, sizeof(Group));
    if(!group)
    {
        fprintf(stderr, "error : unable to calloc\n");
        return 0;
    }
    
    /* 从配置文件读取聊天室信息 */
    for(grid = 0; grid < ngroups; grid++)
    {
        /* 读取聊天室名和容量 */
        if(fscanf(fp, "%s %d", name, &capa) != 2)
        {
            fprintf(stderr, "error : no info on group %d\n", grid + 1);
            return 0;
        }
        group[grid].name = strdup(name);
        group[grid].capa = capa;
        group[grid].occu = 0;
        group[grid].mems = NULL;
    }
    return 1;

}

/*把所有聊天室信息发给客户端 */
int listgroups(int sock)
{
    int grid;
    char pktbufr[MAXPKTLEN];
    char* bufrptr;
    long bufrlen;
    #if DEBUG
    printf("tag 03\n");
    #endif
    /*  每一块信息在字符串中用 NULL 分割 */
    bufrptr = pktbufr;
    for(grid = 0; grid < ngroups; grid++)
    {
        #if DEBUG
        printf("DEBUG: 01 bufrlen = %d\n",bufrptr - pktbufr);
        #endif

        /* 获取聊天室名字 */
        sprintf(bufrptr, "%s", group[grid].name);
        bufrptr += strlen(bufrptr) + 1;

        #if DEBUG
        printf("DEBUG: 02 bufrlen = %d name = %s\n",bufrptr - pktbufr, pktbufr);
        #endif

        /* 获取聊天室容量 */
        sprintf(bufrptr, "%d", group[grid].capa);
        bufrptr += strlen(bufrptr) + 1;

        #if DEBUG
        printf("DEBUG: 03 bufrlen = %d\n",bufrptr - pktbufr);
        #endif

        /* 获取聊天室占有率 */
        sprintf(bufrptr, "%d", group[grid].occu);
        bufrptr += strlen(bufrptr) + 1;

        #if DEBUG
        printf("DEBUG: 04 bufrlen = %d\n",bufrptr - pktbufr);
        #endif
    }
    bufrlen = bufrptr - pktbufr;
    #if DEBUG
    printf("DEBUG 01: bufrlen = %d\n", bufrlen);
    #endif
    /* 发送消息给回复客户机的请求 */
    sendpkt(sock, LIST_GROUPS, bufrlen, pktbufr);
    return 1;
}

/* 加入聊天室 */
int joingroup(int sock, char *gname, char *mname)
{
    int  grid;
    Member* memb;

    /* 根据聊天室名获得聊天室 ID */
    grid = FindGroup(gname);
    if(grid == -1)
    {
        char *errmsg = "no such group";
        sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg);
        return 0;
    }
    /* 检查聊天室成员名字是否已被占用 */
    memb = FindMemberByName(mname);

    /* 如果聊天室成员名已存在， 则返回错误消息 */
    if(memb)
    {
        char *errmsg = "member name already exists";
        sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg);
        return 0;
    }

    /* 检查聊天室是否已满 */
    if (group[grid].capa == group[grid].occu)
    {
        char *errmsg = "room is full";
        sendpkt(sock, JOIN_REJECTED, strlen(errmsg), errmsg);
        return 0;
    }

    /* 为聊天室新成员申请内存空间来存储成员信息 */
    memb = (Member *) calloc(1, sizeof(Member));
    if(!memb)
    {
        fprintf(stderr, "error : ubable to calloc\n");
        cleanup();
    }
    memb->name = strdup(mname);
    memb->sock = sock;
    memb->grid = grid;
    memb->prev = NULL;
    memb->next = group[grid].mems;
    if(group[grid].mems)
    {
        group[grid].mems->prev = memb;
    }
    group[grid].mems = memb;
    printf("admin: '%s' joined '%s' \n", mname, gname);

    /* 更新聊天室的在线人数 */
    group[grid].occu++;
    sendpkt(sock, JOIN_ACCEPTED, 0, NULL);
    return 1;

}


/* 离开聊天室 */
int leavegroup(int sock)
{
	Member *memb;

	/* 得到聊天室成员信息 */
	memb = FindMemberBySock(sock);
	if (!memb) 
		return(0);

	/* 从聊天室信息结构中删除 memb 成员 */
	if (memb->next) 
		memb->next->prev = memb->prev; /* 在聊天室成员链表的尾部 */

	/* remove from ... */
	if (group[memb->grid].mems == memb) /* 在聊天室成员链表的头部 */
		group[memb->grid].mems = memb->next;

	else 
		memb->prev->next = memb->next; /* 在聊天室成员链表的中部 */
	
	printf("admin: '%s' left '%s'\n",
		memb->name, group[memb->grid].name);

	/* 更新聊天室的占有率 */
	group[memb->grid].occu--;

	/* 释放内存 */
	free(memb->name);
	free(memb);
	return(1);
}

/* 把成员的消息发送给其他聊天室成员 */
int relaymsg(int sock, char *text)
{
	Member *memb;
	Member *sender;
	char pktbufr[MAXPKTLEN];
	char *bufrptr;
	long bufrlen;

	/* 根据 socket 描述符获得该聊天室成员的信息 */
	sender = FindMemberBySock(sock);
	if (!sender)
	{
		fprintf(stderr, "strange: no member at %d\n", sock);
		return(0);
	}

	/* 把发送者的姓名添加到消息文本前边 */
	bufrptr = pktbufr;
	strcpy(bufrptr,sender->name);
	bufrptr += strlen(bufrptr) + 1;
	strcpy(bufrptr, text);
	bufrptr += strlen(bufrptr) + 1;
	bufrlen = bufrptr - pktbufr;

	/* 广播该消息给该成员所在聊天室的其他成员 */
	for (memb = group[sender->grid].mems; memb; memb = memb->next)
	{
		/* 跳过发送者 */
		if (memb->sock == sock) 
			continue;
		sendpkt(memb->sock, USER_TEXT, bufrlen, pktbufr); /* 给聊天室其他成员  发送消息（TCP是全双工的） */
	}
	printf("%s: %s", sender->name, text);
	return(1);
}
