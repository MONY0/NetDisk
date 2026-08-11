#include "block_epoll_net.h"
#include <new>

static bool recvAllFd(int fd,char *buf,int total)
{
    int received=0;
    while(received<total)
    {
        ssize_t ret=recv(fd,buf+received,total-received,0);
        if(ret>0){ received+=(int)ret; continue; }
        if(ret==0)return false;
        if(errno==EINTR)continue;
        return false;
    }
    return true;
}

static bool sendAllFd(int fd,const char *buf,int total)
{
    int sent=0;
    while(sent<total)
    {
        ssize_t ret=send(fd,buf+sent,total-sent,MSG_NOSIGNAL);
        if(ret>0){ sent+=(int)ret; continue; }
        if(ret<0 && errno==EINTR)continue;
        return false;
    }
    return true;
}


bool Block_Epoll_Net::InitNet(int port, void (*recv_callback)(int, char *, int))
{
    m_recv_callback = recv_callback;
    InitThreadPool();

    int flags = 1;
    int ret = 0;
    m_listenEv = new myevent_s(this);

    //creat a tcp socket
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( m_listenfd  == -1 ){
        perror("create socket error");
        return false;
    }

    //set REUSERADDR
    ret = setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&flags, sizeof(flags));
    if ( ret == -1 ){
        perror("setsockopt error");
        return false;
    }

    //监听套接字m_listenfd 采用 LT 非阻塞模式
    //set NONBLOCK
    setNonBlockFd( m_listenfd );

    struct sockaddr_in local_addr;
    bzero( &local_addr , sizeof(sockaddr_in) );
    //set address
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    //bind addr
    ret = bind(m_listenfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in));
    if( ret == -1 ) {
        perror("bind error");
        close(m_listenfd);
        return false;
    }

    if (listen(m_listenfd, 128) == -1 ){
        perror("listen error");
        close(m_listenfd);
        return false;
    }

    //create epoll
    m_epoll_fd = epoll_create( MAX_EVENTS );

    m_listenEv->eventset(m_listenfd ,m_epoll_fd );
    //将监听套接字 添加到epoll中 , 模式LT 非阻塞
    m_listenEv->eventadd( EPOLLIN);


    return true;
}

bool Block_Epoll_Net::InitThreadPool()
{

    m_threadpool = new thread_pool;

    //创建拥有10个线程的线程池 最大线程数200 环形队列最大值50000
    if( (m_threadpool->Pool_create(200,10,50000)) == false )
    {
        perror("Create Thread_Pool Failed:");
        exit(-1);
    }

    return true;
}

void Block_Epoll_Net::EventLoop()
{
    printf("EventLoop:server running\n");
    int  i = 0;
    while (1) {

        /* 等待事件发生 */
        int nfd = epoll_wait( m_epoll_fd, events, MAX_EVENTS+1, 1000);
        if (nfd < 0) {
            printf("epoll_wait error, exit\n");
//            break;
            continue;
        }
        for (i = 0; i < nfd; i++) {
            struct myevent_s *ev = (struct myevent_s *)events[i].data.ptr;
            int fd = ev->fd;
            if ( (events[i].events & EPOLLIN) ) {
                if( fd == m_listenfd )
                    accept_event();
                else
                    recv_event( ev );
            }
            if ((events[i].events & EPOLLOUT) ) {
                epollout_event( ev );
            }
        }
    }
}

void Block_Epoll_Net::accept_event()
{
    struct sockaddr_in caddr;
    socklen_t len = sizeof(caddr);
    int clientfd ;
    if ((clientfd = accept(m_listenfd, (struct sockaddr *)&caddr, &len)) == -1) {
        if (errno != EAGAIN && errno != EINTR) {
            /* 暂时不做出错处理 */
        }
        printf("%s: accept, %s\n", __func__, strerror(errno));
        return;
    }
//    //客户端接收使用阻塞模式 更简单 适合入门
//    //设置非阻塞
//    setNonBlockFd( clientfd );

    //设置接收缓冲区大小
    setRecvBufSize( clientfd );
    //设置发送缓冲区大小
    setSendBufSize( clientfd );
    //设置 无延迟
    setNoDelay( clientfd );
    struct timeval timeout;
    timeout.tv_sec=30;
    timeout.tv_usec=0;
    setsockopt(clientfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(clientfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));

    myevent_s * clientEv = new myevent_s(this);
    clientEv->eventset( clientfd , m_epoll_fd );
    // 使用EPOLLONESHOT epoll监听不会重复检测 避免多线程并发 使得同一个套接字接收是排队的
    clientEv->eventadd(  EPOLLIN/*|EPOLLET*/|EPOLLONESHOT );

    //m_mapSockfdToEvent[clientfd] = clientEv;
    m_mapSockfdToEvent.insert(clientfd , clientEv);

    printf("new connect [%s:%d][time:%ld] \n",
           inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port), time(NULL) );
    return;
}




void Block_Epoll_Net::recv_event( myevent_s *ev)
{
    //接收事件采用线程池 多线程处理, 由于使用EPOLLONESHOT , 避免同一套机字的线程并发问题
     m_threadpool->Producer_add(  recv_task , (void*) ev );
}

void* Block_Epoll_Net::recv_task(void* arg)
{
    myevent_s *ev=(myevent_s*)arg;
    if(!ev || !ev->pNet)return NULL;
    Block_Epoll_Net *pthis=ev->pNet;

    uint32_t networkSize=0;
    uint32_t packSize=0;
    if(!recvAllFd(ev->fd,(char*)&networkSize,sizeof(networkSize)))goto recv_error;
    packSize=ntohl(networkSize);
    if(packSize<sizeof(int) || packSize>_DEF_MAX_PACKET_SIZE)goto recv_error;

    {
        char *buf=new(std::nothrow) char[packSize];
        if(!buf)goto recv_error;
        if(!recvAllFd(ev->fd,buf,(int)packSize))
        {
            delete []buf;
            goto recv_error;
        }
        DataBuffer *buffer=new(std::nothrow) DataBuffer(pthis,ev->fd,buf,(int)packSize);
        if(!buffer || pthis->m_threadpool->Producer_add(Buffer_Deal,buffer)!=0)
        {
            delete []buf;
            delete buffer;
            goto recv_error;
        }
    }
    ev->eventadd(ev->events);
    return NULL;

recv_error:
    ev->eventdel();
    close(ev->fd);
    pthis->m_mapSockfdToEvent.erase(ev->fd);
    delete ev;
    return NULL;

}

void * Block_Epoll_Net::Buffer_Deal( void * arg )
{
    DataBuffer * buffer = (DataBuffer *)arg;
    if( !buffer ) return NULL;

    buffer->pNet->m_recv_callback(buffer->sockfd,buffer->buf,buffer->nlen);

 //   printf("pszbuf = %p \n",buffer->buf);
    if(buffer->buf != NULL)
    {
        delete [] buffer->buf;
        buffer->buf = NULL;
    }
    delete buffer;
    return 0;
}

void Block_Epoll_Net::epollout_event( myevent_s *ev )
{
    // epoll LT模式 阻塞模式 发送阻塞 , 不用监听EPOLLOUT事件
}



int Block_Epoll_Net::SendData(int fd, char *szbuf, int nlen)
{
    // 发送不可分割 避免多线程并发
    // 先包大小, 再数据包 , 一次放入缓冲区

    /*
     +--------------+------------------+---------------------+
     |<-- 4bytes -->|<-- 4bytes协议头 ->|<--  协议其他内容    ->|
     +--------------+------------------+---------------------+
     |<- packsize ->|<------------ 数据包 struct ------------>|
     * */

    if(fd<0 || !szbuf || nlen<(int)sizeof(int) || nlen>_DEF_MAX_PACKET_SIZE)return -1;
    uint32_t networkSize=htonl((uint32_t)nlen);
    vector<char> packet(sizeof(networkSize)+nlen);
    memcpy(packet.data(),&networkSize,sizeof(networkSize));
    memcpy(packet.data()+sizeof(networkSize),szbuf,nlen);

    pthread_mutex_lock(&m_sendLock);
    bool ok=sendAllFd(fd,packet.data(),(int)packet.size());
    pthread_mutex_unlock(&m_sendLock);
    return ok ? nlen : -1;
}

void Block_Epoll_Net::setNonBlockFd(int fd)
{
    int flags = 0;
    flags = fcntl(fd, F_GETFL, 0);
    int ret = fcntl(fd, F_SETFL, flags|O_NONBLOCK);
    if( ret == -1)
        perror("setNonBlockFd fail:");
}

void Block_Epoll_Net::setRecvBufSize( int fd)
{
    //接收缓冲区
    int nRecvBuf = 256*1024;//设置为 256 K
    setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
}

void Block_Epoll_Net::setSendBufSize( int fd)
{
    //发送缓冲区
    int nSendBuf=128*1024;//设置为 128 K
    setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
}

#include<netinet/tcp.h>
void Block_Epoll_Net::setNoDelay( int fd)
{
    //nodelay
    int value = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY ,(char *)&value, sizeof(int));
}
