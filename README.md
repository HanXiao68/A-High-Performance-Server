# libevent
多线程
！[libevent的核心代码部分](https://github.com/HanXiao68/libevent/blob/master/libevent%E6%A0%B8%E5%BF%83%E9%83%A8%E5%88%86.pdf)

[![Build Status](https://travis-ci.org/linyacool/WebServer.svg?branch=master)](https://travis-ci.org/linyacool/WebServer)
[![license](https://img.shields.io/github/license/mashape/apistatus.svg)](https://opensource.org/licenses/MIT)

# 目录

| Chapter 0 | Chapter 1 | Chapter 2 | Chapter 3| Chapter 4 |Chapter 5|
| :---------:| :---------: | :---------: | :---------: | :--------: |:--------:|
|[编译和安装](#pro)|[文件处理](#file)|[性能分析](#sysinfo)|[网络工具](#net)|[其他](#other)|
---


memcache的源码，它的io就是一个非常典型的基于libevent的多线程io服务

因此，Libevent允许进行可移植的应用程序开发，并提供操作系统上可用的最可伸缩的事件通知机制。Libevent还可以用于多线程应用程序，方法是隔离每个事件库以便只有一个线程访问它，或者锁定对单个共享事件库的访问。

Libevent还为缓冲网络IO提供了一个复杂的框架，支持套接字、过滤器、速率限制、SSL、零拷贝文件传输和IOCP。Libevent包括对几个有用协议的支持，包括DNS、HTTP和一个最小的RPC框架。

memcache基于c语言，他的线程池基于管道，windows不支持管道

线程池和线程通信 通过管道，每个线程有一个eventbase。参考memcache。

c++中static 不需要对象就可以访问

用vector放线程池，可以下标操作，list不能下标

.h文件中不加命名空间，少加头文件。 不知道调用者的情况
头文件 头文件随便加


阻塞式

select：跨平台--Linux和window都支持。每次都要从用户空间拷贝到内核空间。 遍历整个fd_set.  接口简单，开发简单；监听的fd数量有限制。

poll：和select

epoll：只拷贝一次，共享内存交互。内部基于红黑树特点。
        不用全部复制，返回双向链表
        只通过epoll wait返回红黑树中 fd状态发生变化的。
    缺点：不支持windows

    LT水平触发：如果事件没有处理，系统一直通知。epoll wait始终能获取到未处理的链表。
    ET边沿触发：只通知一次，每当状态发生变化时，触发一个事件
    两者本质区别是：取出来后，ET不删红黑树中。下一次取还是这个。ET取完以后，就删除了
iocp：不支持linux

在libevent中的级别：epoll---poll---select--iocp(需要指定)

---

libevent 开发
    环境配置初始化 event base_new
    evutil socket函数封装
    事件IO处理：event_new  需要传递socket或者其他fd
    缓冲IO：bufferevent网络接口（不用传递socket） 读写都在缓冲进行
    循环：event_base_dispatch


使用libevent的关键点是处理并发 事件IO，或者跨平台的socket


测试libevent在linux下支持的 IO多路复用模式


为什么non-blocking网络编程中应用层buffer是必需的？

non-blocking IO的核心思想是避免阻塞在read()和write()或其他IO系统调用上，这样可以最大限度的复用thread-of-control，让一个线程服务于多个socket连接。IO线程只能阻塞在IO多路复用函数上，例如select/poll/epoll_wait.。因此，应用层的缓冲是必需的，每个TCPsocket都有输入buffer和输出buffer。

为什么要限制并发连接数？建立线程池？
   
   一方面，我们不希望服务程序超载
    另一方面，因为file descripter（文件描述符）是稀缺资源。如果fd耗尽，结果跟 调用malloc()失败，抛出bad_malloc 错误的严重程度一样。

---
<img src="https://github.com/HanXiao68/libevent/blob/master/image/reactor.png" width="575"/>
# Reactor模式

    要求主线程（IO处理单元）只负责监听文件描述符fd上是否有时间发生，有的话立即将事件通知工作线程（逻辑单元）。除此之外，主线程不做任何其他实质性的工作。
    读写数据，接受新的连接，以及处理客户请求均在工作线程中完成。

    使用同步IO模型（以epoll为例）实现的Reactor模式的工作流程是：
        1.主线程往epoll内核事件表中注册socket上的读就绪事件。
        2.主线程调用epoll_wait等待socket上有数据可读。
        3.当socket上有数据可读时，epoll_wait通知主线程。主线程将socket可读事件放入请求队列。
        4.睡眠在请求队列上的某个工作线程被唤醒，他从socket读取数据，并处理客户请求，然后往epoll内核事件表中注册该socket上的写就绪事件。
        5.主线程调用epoll_wait等待socket可写。
        6.当socket可写时，epoll_wait通知主线程。主线程将socket可写事件放入请求队列。
        7.睡眠在请求队列撒花姑娘的某个工作线程被唤醒，它往socket上写入服务器处理客户请求的结果。

当socket上有数据可写时，epoll_wait通知主线程。主线程将socket可写事件放入请求队列。
---

# 线程池ThreadPool

计算机一般8核16线程，在32线程以下

	event base 可以设置加锁。用IO多路复用
        
	window不支持管道。memcache是基于管道。
        
	线程池：memcache的源码：Thread.c的文件 线程池的实现：分发线程，初始化线程，调用linevent时间的分发，然后阻塞。
        
	x threadPool:
        
			来一个连接，发一个线程
                        
			线程数量和CPU线程数一样就行。8核16线程
                        
			非阻塞方式，
                        
			先初始化10个线程。初始化和创建好
                        
			分发：轮询的方式。 dispatch（） memcache也是采用这种方式
                        
				线程添加到任务队列中 参考memcache  发一个管道消息，用的epoll。
                                
				处理：交给libevent。
                                
	xthread：
		setup（）安装线程，初始化libevent事件
		创建管道事件。线程池和线程通信，通过管道
		每个线程用一个event_base
		start():
			kaishi 线程。
			启动线程。用的c++11的thread。
		main():
			线程函数，调用libevent的事件循环。
		addtask():
			添加到任务队列。一个线程可以同时处理多个任务(可以是一个http连接。或者处理各传感器信息。)，共用一个event_base
			用了锁，保证线程安全
		activate():
			向线程发出激活的管道消息。“传感器连接激活”
		notify（）：
			收到线程池发出的激活消息；   获取待处理任务 并处理（LT ET）
	Xtask:
		virtual	bool init() =0 纯虚函数作为初始化接口
		每个具体任务继承接口，自己去实现。	
		每个任务有 	






