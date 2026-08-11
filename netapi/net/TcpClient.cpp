#include "TcpClient.h"


#include"INetMediator.h"
#include"packdef.h"
#include<vector>
#include<mutex>
#include<new>

static bool recvAllSocket(SOCKET fd,char *buf,int total)
{
    int received=0;
    while(received<total)
    {
        int ret=recv(fd,buf+received,total-received,0);
        if(ret>0){ received+=ret; continue; }
        if(ret==0)return false;
        int error=WSAGetLastError();
        if(error==WSAEINTR)continue;
        return false;
    }
    return true;
}

static bool sendAllSocket(SOCKET fd,const char *buf,int total)
{
    int sent=0;
    while(sent<total)
    {
        int ret=send(fd,buf+sent,total-sent,0);
        if(ret>0){ sent+=ret; continue; }
        int error=WSAGetLastError();
        if(error==WSAEINTR)continue;
        return false;
    }
    return true;
}



TcpClient::TcpClient( INetMediator * pMediator ):m_sock( INVALID_SOCKET ),m_isStop(false)
{
	m_pMediator = pMediator;
    m_isConnected = false;
	m_hThreadHandle = NULL;
}

TcpClient::~TcpClient()  //使用时, 父类指针指向子类, 使用虚析构
{
    TcpClient::UnInitNet();
}
//初始化网络	//加载库 创建套接字 绑定
bool TcpClient::InitNet(const char *szBufIP, unsigned short port)
{
    if( !m_isLoadlib )
    {
        //1.做个小买卖 炸串 --加载库
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
        wVersionRequested = MAKEWORD(2, 2);

        err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0) {
            return false;
        }

        if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
            WSACleanup();
            return false;
        }
        m_isLoadlib = true;
    }

    m_isConnected = false;

	//2.雇人-- 创建套接字 进程与外界网络通信需要的接口 决定了与外界通讯的方式(tcp udp)
	m_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ); // ipv4 udp
	if ( m_sock == INVALID_SOCKET) {
		WSACleanup();
        return false;
	}

	//3. 连接服务器
	sockaddr_in addr;
	addr.sin_family = AF_INET ;
    addr.sin_addr.S_un.S_addr = inet_addr( szBufIP );  /*inet_addr("192.168.31.115")*/ ;  //绑定任意网卡
    addr.sin_port = htons( port );  //htons 转换为网络字节序 大端存储  43232

	
	if( connect( m_sock ,(const sockaddr* ) &addr , sizeof(addr) ) == SOCKET_ERROR )
	{
		UnInitNet();
        return false;
	}

    //设置客户端发送缓冲区
    //     int nSendBuf=64*1024;//设置为64K
    //     setsockopt(m_sock,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
    //设置客户端接收缓冲区
    //     int nRecvBuf=64*1024;//设置为64K
    //     setsockopt(m_sock,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));

//    DWORD nNetTimeout= 300;//1秒 - 1000

//    //发送时限 , 超时退出避免长时间阻塞
//    setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&nNetTimeout, sizeof(DWORD));

    //禁用 TCP-NODELAY
    int value = 1;
    setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&value, sizeof(int));


    m_isConnected = true;
	// 收数据 -- 创建线程 CreateThread  WinAPI  strcpy  C/C++ RunTime 库函数 创建内存块
	m_hThreadHandle = (HANDLE)_beginthreadex(  NULL, 0 ,&RecvThread ,this , 0 , NULL );     //( CreateThread 创建内存块 )
	//_endthreadex(); -- 回收内存块      //( ExitThread 不回收内存块 ) --内存泄露

	return true;
}

unsigned int __stdcall TcpClient::RecvThread( void * lpvoid)
{
	TcpClient* pthis = (TcpClient*) lpvoid;
	pthis->RecvData();
	
	return 0;
}

//关闭网络
void TcpClient::UnInitNet()
{
	m_isStop = true ; // 尝试退出线程循环 
	if(m_sock!=INVALID_SOCKET)shutdown(m_sock,SD_BOTH);
	if( m_hThreadHandle )
	{
		if( WaitForSingleObject(m_hThreadHandle , 100 ) == WAIT_TIMEOUT )
		{
			TerminateThread( m_hThreadHandle , -1 );
		}
		CloseHandle(m_hThreadHandle);
		m_hThreadHandle = NULL;
	}
	if( m_sock && m_sock != INVALID_SOCKET )
		closesocket(m_sock);

    if( m_isLoadlib )
    {
        WSACleanup();
        m_isLoadlib = false;
    }
} 
//发送 : 同时兼容tcp udp 
bool TcpClient::SendData(unsigned int lSendIP , char* szbuf , int nlen )
{
    (void)lSendIP;
    if(!szbuf || nlen<(int)sizeof(PackType) ||
       nlen>_DEF_MAX_PACKET_SIZE || m_sock==INVALID_SOCKET)return false;

    uint32_t networkSize=htonl((uint32_t)nlen);
    std::vector<char> packet(sizeof(networkSize)+nlen);
    memcpy(packet.data(),&networkSize,sizeof(networkSize));
    memcpy(packet.data()+sizeof(networkSize),szbuf,nlen);

    std::lock_guard<std::mutex> locker(m_sendMutex);
    return sendAllSocket(m_sock,packet.data(),(int)packet.size());
}
//接收

void TcpClient::RecvData()
{
	while( !m_isStop )
	{
		uint32_t networkSize=0;
        if(!recvAllSocket(m_sock,(char*)&networkSize,sizeof(networkSize)))break;
        uint32_t packSize=ntohl(networkSize);
        if(packSize<sizeof(PackType) || packSize>_DEF_MAX_PACKET_SIZE)break;

        char *buf=new(std::nothrow) char[packSize];
        if(!buf)break;
        if(!recvAllSocket(m_sock,buf,(int)packSize))
        {
            delete []buf;
            break;
        }
		m_pMediator->DealData(m_sock,buf,(int)packSize);
	}
	m_isConnected=false;
	if(!m_isStop)m_pMediator->disConnect();
}

bool TcpClient::IsConnected()
{
    return  m_isConnected;
}

