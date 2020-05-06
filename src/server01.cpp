#include <event2/event.h>
#include <event2/listener.h>
#include <iostream>
using namespace std;
#define SPORT 5001
int main()
{

//忽略管道信号，发送数据给已经关闭的socket，程序会出错。
	if(signal(SIGPIPE,SIG_IGN) = SIG_ERR){
		return 1；
	}
	cout<<"test server" <<endl;

    std::cout << "test libevent!\n"; 
	//创建libevent的上下文
	event_base * base = event_base_new();
	if (base)
	{
		cout << "event_base_new success!" << endl;
	}

	//监听端口
	//socket, bind   listen 绑定事件
	sockaddr_in sin;
	memset(&sin,0,sizeof(sin)); //把ip地址清零。相当于所有地址链接都能进来
	sin.sin_family =AF_INET;//设置为ipv4协议
	sin.sin_port = htons(SPORT); //主机字节序列设置为网络字节序
	evconnlistener *ev = evconnlistener_new_bind(base, //libevent的上下文
		listen_cb //接受到连接的回调函数 
		，base//回调函数获得参数
		LEV_OPT_REUSEABLE| LEV_OPT_CLOSE_ON_FREE//地址重用 | listen关闭同时关闭socket
		10，//连接队列大小,对应listen函数
		(socket*) &sin	//绑定的地址和端口
		sizeof(sin) //c语言中没有重载，根据对象大小确定类型
		); 	
	
	//事件分发处理
	if(base)
		event_base_dispatch(base);
	if(ev)
		evconnlistener_free(ev);
	if(base)
		event_bse_free(base);
	return 0;
}
