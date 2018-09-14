#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
 
#define MAX_EVENTS 256
#define SERV_PORT	9999
#define LENGTH_OF_LISTEN_QUEUE 10240
#define MAX_RECEIVE 1024
 
/* 每一个客户端数据缓冲区 */
typedef struct client_buf
{
	int  fd;                /* 保存当前客户端的fd */
	char buf[MAX_RECEIVE];  /* 缓冲区 */
}client_buf_s, *client_buf_p;
 
/* 为客户端分配c_buf_s 结构 */
static void* new_client_buf(int fd);
 
/* epoll事件集 */
struct epoll_event evts[MAX_EVENTS];
 
/* 获取一个监听连接的sockfd */
int run_getsockfd(const char*ip, int port);
 
/* 执行epoll逻辑 (创建，添加监听事件，等待返回)*/
void run_epoll(int listen_sockfd);
 
/* 处理客户端事件就绪 */
void run_action(int epollfd, int index);
 
/* 处理客户端连接请求，并添加到epoll模型中 */
void run_accept(int epollfd, int listen_sockfd);

void setnonblocking(int sock)
{
	int opts;
	opts = fcntl(sock, F_GETFL);

	if(opts < 0) {
		perror("fcntl(sock, GETFL)");
		exit(1);
	}

	opts = opts | O_NONBLOCK;

	if(fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(sock, SETFL, opts)");
		exit(1);
	}
}

int start_server()
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  setnonblocking(listenfd);

  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	char *local_addr = "127.0.0.1";
	inet_aton(local_addr, &(serveraddr.sin_addr));
	serveraddr.sin_port = htons(SERV_PORT);

  // 绑定socket和socket地址结构
  if(-1 == (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))))
  {
      perror("Server Bind Failed:");
      exit(1);
  }
  // socket监听
  if(-1 == (listen(listenfd, LENGTH_OF_LISTEN_QUEUE)))
  {
      perror("Server Listen Failed:");
      exit(1);
  }

  return listenfd;
}
 
int main(int argc, char** argv)
{
	/* 获取监听socketfd  */
	int listen_sockfd = start_server();
	run_epoll(listen_sockfd);
 
	return 0;
}
 
 

/***********************************************************
* 执行epoll逻辑 (创建，添加监听事件，等待返回)
* @param		listen_sockfd
* @return   
************************************************************/
void run_epoll(int listen_sockfd)
{
	// 1. 创建 epoll FD
	int epollfd = epoll_create(256);
	if( epollfd < 0)
	{
		perror("epoll_create()");
		exit(4);
	}
 
	// 2. 添加监听描述符到模型中
	struct epoll_event evt;
	evt.events = EPOLLIN;
	evt.data.ptr = new_client_buf(listen_sockfd);  /* 分配事件数据结构 */
	epoll_ctl(epollfd, EPOLL_CTL_ADD,listen_sockfd, &evt);
 
	int nfds = 0;        /* 就绪事件个数 */
	int timeout = 2000;  /* 2秒超时  */

  while(1)
	{
		nfds = epoll_wait(epollfd, evts, MAX_EVENTS, timeout );	
		if( nfds < 0)
		{
			perror("epoll_wait()");
			continue;
		}
		else if (nfds == 0)
		{
			//printf("epoll_wait() timeout\n");
		}
		else
		{
			/* 变量处理已经就绪的事件 */
			int idx_check = 0;
			for(; idx_check < nfds; idx_check++)
			{
				client_buf_p cbp  = evts[idx_check].data.ptr;
				/* 监听socket 读事件就绪 */
				if( cbp->fd == listen_sockfd &&	(evts[idx_check].events & EPOLLIN)  )
				{
					/* 接受客户端的请求 */
					run_accept(epollfd, listen_sockfd);
				}/* 客户端socket 事件就绪 */
				else if ( fp->fd != listen_sockfd )
				{
					run_action(epollfd, idx_check);
				}
			}
		}
	}
}

/***********************************************************
* 分配客户端FD数据结构
* @param		fd
* @return   
************************************************************/
static void* new_client_buf(int fd)
{
	client_buf_p ret  = (client_buf_p)malloc(sizeof(client_buf_t));
	assert( ret != NULL);
 
	ret->fd = fd;
	memset(ret->buf, 0, sizeof(ret->buf));
	return ret; 
}
 
/* 处理客户端连接请求，并添加到epoll模型中 */
void run_accept(int epollfd, int listen_sockfd)
{
	
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
 
	int new_sock = accept(listen_sockfd, (struct sockaddr*)&cliaddr, &clilen);
	if(new_sock < 0)
	{
		perror("accept");
		return;
	}
  setnonblocking(new_sock);
	//printf("与客户端连接成功: ip %s port %d \n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
 
	/* 将与客户端连接的sockfd 添加到epoll模型中，并关心读事件 */
	struct epoll_event evt;
	evt.events = EPOLLIN | EPOLLET;
	evt.data.ptr = new_client_buf(new_sock); /* 为每一个客户端链接分配fd和缓冲区 */
	epoll_ctl(epollfd, EPOLL_CTL_ADD, new_sock ,&evt);
}
 
ssize_t socket_send(int sockfd, const char* buffer, size_t buflen) {
  ssize_t tmp;
  size_t total = buflen;
  const char *p = buffer;

  while(1) {
    tmp = send(sockfd, p, total, 0);
    if(tmp < 0) {
      if(errno == EINTR) // 当send收到信号时,可以继续写,但这里返回-1.
        return -1;
      if(errno == EAGAIN) { // 写缓冲队列已满,
        usleep(500); // 延时后再重试.
        continue;
      }
      return -1;
    }

    if((size_t)tmp == total)
      return buflen;

    total -= tmp;
    p += tmp;
  }

  return tmp;  //返回实际发送长度
}

/* 处理客户端事件就绪 */
void run_action(int epollfd, int index)
{
	client_buf_p cbp = evts[index].data.ptr;
 
	/* 读事件就绪 */
	if(evts[index].events & EPOLLIN)
	{
		int ret;
		int recvLen = 0;
		int revMax = MAX_RECEIVE;
	
		while(recvLen < revMax) {
			ret = recv(cbp->fd, (char *)(cbp->buf+recvLen), revMax-recvLen, 0);
			if(ret <= 0) {  //这里表示对端的socket已正常关闭.
					break;
			}
			recvLen = recvLen + ret; //数据接受正常
		}
		//printf("receive data %s , len %d \n",cbp->buf, recvLen);
		if(recvLen > 0) {
			cbp->buf[recvLen] = 0;
			struct epoll_event evt; 
			evt.events = EPOLLOUT | EPOLLET;
			evt.data.ptr = cbp;
			epoll_ctl(epollfd, EPOLL_CTL_MOD, cbp->fd, &evt);
		}
		else if(recvLen <= 0)
		{
			// 关闭socket，删除结点并释放内存 
			//printf("\n客户端退出!\n");
			close(cbp->fd);
			epoll_ctl(epollfd, EPOLL_CTL_DEL, cbp->fd, NULL );
			free(cbp);
		}
	}
	else if(evts[index].events & EPOLLOUT)
	{ 
    /* 写事件就绪 */
		const char* msg = "HTTP/1.1 200 OK\r\n\r\n<html><h1>Hello world ...</h1></html>";
		socket_send(cbp->fd, msg, strlen(msg));
		
		close(cbp->fd);
		epoll_ctl(epollfd, EPOLL_CTL_DEL, cbp->fd, NULL);
		free(cbp);
	}
}
