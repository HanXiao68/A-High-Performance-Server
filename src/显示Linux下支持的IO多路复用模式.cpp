

	//EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST
	//	epoll下有效，防止同一个fd多次激发事件，fd如果做复制会有bug

#include <event2/event.h>
#include <signal.h>
#include <iostream>
using namespace std;

int main()
{

	//忽略管道信号，发送数据给已关闭的socket
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
	

	//创建配置上下文
	event_config *conf = event_config_new();

	//显示支持的网络模式
	//二维数组
	const char **methods =  event_get_supported_methods();//支持的模式：poll select，epoll
	cout << "supported_methods:" << endl;
	for (int i = 0; methods[i]!= NULL; i++)
	{
		cout << methods[i] << endl;//把支持的模式都打印出来。
	}

	//初始化配置libevent上下文
	event_base *base = event_base_new_with_config(conf);

	event_config_free(conf);//清理配置文件

	if (!base)//如果失败，
	{
		cerr << "event_base_new_with_config failed!" << endl;
	}
	else
	{
		cout << "event_base_new_with_config success!" << endl;
		event_base_free(base);//成功了也要清理。
	}

	return 0;
}
