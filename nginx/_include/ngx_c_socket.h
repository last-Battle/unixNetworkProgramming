
#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include <vector>      //vector
#include <list>         // list
#include <sys/epoll.h> //epoll
#include <sys/socket.h>

#include "ngx_comm.h"

// 一些宏定义-----------------------------------------------
#define NGX_LISTEN_BACKLOG  511      // 已完成链接队列，nginx官方给511
#define NGX_MAX_EVENTS      512     // epoll_wait一次最多接收这么多个事件nginx中缺省是512

typedef struct ngx_listening_s  ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s ngx_connection_t, *lpngx_connection_t;
typedef class CSocket           CSocket;

typedef void (CSocket::*ngx_event_handler_pt)(lpngx_connection_t c);    // 定义成员函数指针

// 一些专门用于结构定义放在这里
typedef struct ngx_listening_s  // 和监听端口有关的结构
{
    int             port;       // 监听的端口号
    int             fd;         // 套接字句柄 socket
}ngx_listening_t, *lpngx_listening_t;

// 以下三个结构是非常重要
// 1）该结构表示一个TCP连接【客户端主动发起，nginx服务器被动接收的TCP连接】
struct ngx_connection_s
{
    int                         fd;                 // 套接字句柄socket
    lpngx_listening_t           listening;          // 如果这个链接被分配给了一个监听套接字，那么这个里面就指向监听套接字对应的那个lpngx_listening_t的内存首地址
    
    // ---------------------------*********************
    unsigned                    instance:1;         // 【位域】失效标志位：0 有效， 1 失效 【这个是官方nginx中就有的，具体作用在 ngx_epoll_process_events()中详解】
    uint64_t                    iCurrsequence;      // 引入一个序号，每次分配出去时+1，这种方法也有可能在一定程度上检测错包废包，具体用法后续完善
    struct    sockaddr          s_sockaddr;         // 保存对方地址信息
    // char                        addr_text[100];     // 地址的文本信息，100足够，一般其实如果是IPV4地址，255.255.255.255，其实只需要20个字节就足够

    // 和读有关的标志----------------------------------------
    // uint8_t                     r_ready;            // 读准备好标记
    uint8_t                     w_ready;            // 写准备好标记

    ngx_event_handler_pt        rhandler;           // 读事件的相关处理方法
    ngx_event_handler_pt        whandler;           // 写事件的相关处理方法

    // 和收包有关

    unsigned char               curStat;            // 当前收包的状态
    char                        dataHeadInfo[_DATA_BUFSIZE_];       // 用于保存收到的数据的包头信息
    char                        *precvbuf;                          // 接收数据缓冲区的头指针，对收到不全的包非常有用，看具体应用的代码
    unsigned                    irecvlen;                           // 要接收到多少数据，由这个变量指定，和precvbuf配套使用，具体应用看代码

    bool                        ifnewrecvMem;                       // 如果我们成功的收到了包头，那么我们就要分配内存开始保存，包头+包体内容；
                                                                    // 这个标记用来保存我们是否new过内存，如果new过，是需要进行内存释放的
    char                        *pnewMemPointer;                    // new出来的用于收包的内存首地址，和ifnewrecvMem配合使用


    // ------------------------------------------
    lpngx_connection_t          data;               // 这个是一个指针【等价于传统链表里的next成员：后继指针】，用于指向下一个本类型对象，用于把空闲的连接池对象串起来构成一个单向的链表，方便取用



}


//消息头，引入的目的是当收到数据包时，额外记录一些内容以备将来使用
typedef struct _STRUC_MSG_HEADER
{
	lpngx_connection_t pConn;         //记录对应的链接，注意这是个指针
	uint64_t           iCurrsequence; //收到数据包时记录对应连接的序号，将来能用于比较是否连接已经作废用
	//......其他以后扩展	
}STRUC_MSG_HEADER,*LPSTRUC_MSG_HEADER;

// 每个TCP连接至少需要一个读事件和一个写事件，所以定义事件结构
// typedef struct  ngx_event_s
// {

// }ngx_event_t, *lpngx_event_t;

// -----------------------------**********************************-----------------------------------
// socket相关类
class CSocket
{
    public:
        CSocket();                                          // 构造函数
        virtual ~CSocket();                                 // 虚析构

    public:
        virtual bool Initialize();                          // 初始化函数

    public:
        int ngx_epoll_init();                               // epoll功能初始化
        // void ngx_epoll_listenportstart();                // 监听端口开始工作
        int ngx_epoll_add_event(int fd, int readevent, int writeevent,uint32_t otherflag,uint32_t eventtype,lpngx_connection_t c);
                                                            // epool增加事件
        int ngx_epoll_provess_events(int timer);            // epoll等待接收和处理事件

    private:
        void ReadConf();                                    // 专门用于读各种配置项
        bool ngx_open_listening_sockets();                  // 监听必须的端口【支持多个端口】
        void ngx_close_listening_sockets();                 // 关闭监听套接字
        bool setnonblocking(int sockfd);                    // 设置非阻塞套接字

        // 一些业务处理函数handler
        void ngx_event_accept(lpngx_connection_t oldc);     // 建立新连接
        void ngx_wait_request_handler(lpngx_connection_t c);    // 设置数据来的时候的读处理函数

        void ngx_close_connection(lpngx_connection_t c);   // 用户连入，我们accept()的时候，得到的socket在处理中产生失败，则资源用这个函数进行释放[改为通用连接关闭函数]
                                                                    // 因为这里涉及到好几个要释放的资源，所以写成函数

        ssize_t recvproc(lpngx_connection_t c, char *buff, ssize_t buflen);             // 接收从客户端来的数据专用函数
        void ngx_wait_request_handler_proc_p1(lpngx_connection_t c);                    // 包头接收完整后的处理函数，这里称之为包处理阶段1，写成函数方便复用
        void ngx_wait_request_handler_proc_plast(lpngx_connection_t c);                 // 收到一个完整的包后的处理函数
        void inMsgRecvQueue(char *buf);                                                 // 收到一个完整的消息后，入消息队列
        void tmpoutMsgRecvQueue();      // 临时清除队列中的消息的函数，测试使用，将来会进行删除
        void clearMsgRecvQueue();                                                       // 清理消息队列

        
        // 获取对端信息相关
        size_t ngx_sock_ntop(struct sockaddr *sa, int port, u_char *text,size_t len);   // 根据参数1给定的信息，获取地址端口字符串，返回这个字符串的长度

        // 连接池 或 连接相关
        lpngx_connection_t ngx_get_connection(int isock);           // 从连接池中获取一个空闲连接
        void ngx_free_connection(lpngx_connection_t c);             // 归还参数c所代表的连接到连接池中


    private:
        int                             m_worker_connections;   // epoll连接的最大项数
        int                             m_ListenPortCount;      // 监听的端口数量
        int                             m_epollhandle;          // epoll_create返回的句柄


        // 和连接池有关的
        lpngx_connection_t              m_pconnections;         // 注意这里是一个指针，其实这里是个连接池的首地址
        lpngx_connection_t              m_pfree_connections;    // 空闲连接链表头，连接池中总是有某些连接被占用，为了能在池中快速找到一个空闲的连接，把空闲的连接专门用该成员进行记录
                                                                // 串成一串，其实这里指向的都是m_pconnections连接池里的没有被使用的成员

        // lpngx_event_t                m_pread_events;         // 指针，读事件数组
        // lpngx_event_t                m_pwrite_events;        // 指针，写事件数组

        int                             m_connection_n;         // 当前进程中所有连接对象的总数【连接池大小】
        int                             m_free_connection_n;    // 连接池中可用连接的总数

        std::vector<lpngx_listening_t>  m_ListenSocketList;     // 监听套接字队列

        struct  epoll_event             m_events[NGX_MAX_EVENTS];   // 用于在epoll_wait()中承载返回的所发生的事件

        //一些和网络通讯有关的成员变量
        size_t                         m_iLenPkgHeader;                    //sizeof(COMM_PKG_HEADER);		
        size_t                         m_iLenMsgHeader;                    //sizeof(STRUC_MSG_HEADER);
        //消息队列
        std::list<char *>              m_MsgRecvQueue;                     //接收数据消息队列 
};

#endif