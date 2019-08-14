
/* 聊天室客户端程序 */
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
#include <stdlib.h>

#include "Linker.h"

#define QUIT_STRING "/end"
#define DEBUG 0

/* 打印聊天室名单 */
void showgroups(long lent, char *text)
{
    char *tptr;
    tptr = text;
    printf("%18s %19s %19s\n", "\u804a\u5929\u5ba4", "\u4eba\u6570\u4e0a\u9650", "\u5728\u7ebf\u4eba\u6570");
    #if DEBUG
    printf("DEBUG 01: lent = %d text = %s\n", lent, text);
    #endif
	while (tptr < text + lent) 
	{
		char *name, *capa, *occu;

		name = tptr;
		tptr = name + strlen(name) + 1;
		capa = tptr;
		tptr = capa + strlen(capa) + 1;
		occu = tptr;
		tptr = occu + strlen(occu) + 1;

		printf("%15s %15s %15s\n", name, capa, occu);
	}
}

/* 加入聊天室 */
int joinagroup(int sock)
{
    Packet* pkt;
    char bufr[MAXPKTLEN];
    char *bufrptr;
    int bufrlen;
    char *gname;
    char *mname;

    /* 请求聊天室信息 */
    sendpkt(sock, LIST_GROUPS, 0, NULL);
    /*接受聊天室信息回复 */
    pkt = recvpkt(sock);
    if(!pkt)
    {
        fprintf(stderr,"error : server died\n");
        exit(1);
    }
    if(pkt->type != LIST_GROUPS)
    {
        fprintf(stderr, "error: unexpected reply from server\n");
        exit(1);
    }
    /* 显示聊天室 */
    showgroups(pkt->lent, pkt->text);

    /* 从标准输入读入聊天室名 */
    printf("which group?");
    fgets(bufr, MAXPKTLEN, stdin);
    #if DEBUG
        printf("DEBUG %s: strlen(bufr) = %d, bufr = %s\n",__func__, strlen(bufr), bufr);
    #endif
    bufr[strlen(bufr) - 1] = '\0';
    #if DEBUG
        printf("DEBUG %s: strlen(bufr) = %d bufr = %s\n",__func__, strlen(bufr),bufr);
    #endif
    /* 此时可能用户想退出 */
    if(strcmp(bufr, "") == 0 || strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0)
    {
        close(sock);
        exit(0);
    }
    gname = strdup(bufr);

    /* 读入成员名字 */
	printf("what nickname? ");
	fgets(bufr, MAXPKTLEN, stdin);
	bufr[strlen(bufr) - 1] = '\0';
    #if DEBUG
        printf("DEBUG %s: strlen(bufr) = %d bufr = %s\n",__func__, strlen(bufr),bufr);
    #endif
	/* 此时可能用户想退出 */
	if (strcmp(bufr, "") == 0
			|| strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0) 
	{
		close(sock);
		exit(0);
	}
	mname = strdup(bufr);

    #if DEBUG
        printf("DEBUG %s: test 01\n",__func__);
    #endif

	/* 发送加入聊天室的信息 */
	bufrptr = bufr;
	strcpy(bufrptr, gname);
	bufrptr += strlen(bufrptr) + 1;
	strcpy(bufrptr, mname);
	bufrptr += strlen(bufrptr) + 1;
	bufrlen = bufrptr - bufr;

    #if DEBUG
        printf("DEBUG %s: test 02\n",__func__);
    #endif

	sendpkt(sock, JOIN_GROUP, bufrlen, bufr);

    #if DEBUG
        printf("DEBUG %s: test 03\n",__func__);
    #endif

	/* 读取来自服务器的回复 */
	pkt = recvpkt(sock);
	if (!pkt) 
	{
		fprintf(stderr, "error: server died\n");
		exit(1);
	}
	if (pkt->type != JOIN_ACCEPTED && pkt->type != JOIN_REJECTED) 
	{
		fprintf(stderr, "error: unexpected reply from server\n");
		exit(1);
	}

	/* 如果拒绝显示其原因 */
	if (pkt->type == JOIN_REJECTED)
	{
		printf("admin: %s\n", pkt->text);
		free(gname);
		free(mname);
		return (0);
	}
	else /* 成功加入 */
	{
		printf("admin: joined '%s' as '%s'\n", gname, mname);
		free(gname);
		free(mname);
		return (1);
	}
}

/* 主函数入口 */
int main(int argc, char **argv)
{
    int sock;
    if(argc != 1)
    {
        fprintf(stderr, "usage : %s\n", argv[0]);
        exit(1);
    }

    /* 与服务器连接 */
    sock = hooktoserver();
    if(sock == -1)
        exit(1);

    fflush(stdout); /* 清楚标准输出缓冲区 */
    fd_set clientfds, tempfds;
    FD_ZERO(&clientfds);
    FD_ZERO(&tempfds);
    FD_SET(sock, &clientfds);
    FD_SET(0, &clientfds);

    while(1)
    {
        /* 加入聊天室 */
        if(!joinagroup(sock))
        {
            continue;
        }

        /* 保持聊天状态 */
        while(1)
        {
            /* 调用select 函数同时检测键盘和服务器信息 */
            tempfds = clientfds;
            if(select(FD_SETSIZE, &tempfds, NULL, NULL, NULL) == -1)
            {
                perror("select");
				exit(4);
            }
            if(FD_ISSET(sock, &tempfds))
            {
                Packet *pkt;
                pkt = recvpkt(sock);
                if(!pkt)
                {
                    fprintf(stderr, "error: server died\n");
                    exit(1);
                }
                /* 显示消息文本 */
                if(pkt->type != USER_TEXT)
                {
                    fprintf(stderr, "error : unexpected reply from server\n");
                    exit(0);
                }
                printf("%s: %s", pkt->text, pkt->text + strlen(pkt->text) + 1);
                freepkt(pkt);
            }
            if (FD_ISSET(0,&tempfds)) 
			{
				char bufr[MAXPKTLEN];

				fgets(bufr, MAXPKTLEN, stdin);
				if (strncmp(bufr, QUIT_STRING, strlen(QUIT_STRING)) == 0) 
				{
					/* 退出聊天室 */
					sendpkt(sock, LEAVE_GROUP, 0, NULL);
					break;
				}

				/* 发送消息文本到服务器 */
				sendpkt(sock, USER_TEXT, strlen(bufr) + 1, bufr);
			}
        }
    }
}