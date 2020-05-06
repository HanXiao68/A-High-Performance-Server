#include<iostream>
#include<event2/event.h>
#include<signal.h>
#include<thread>

using namespace std;

static timeval t1={1,0};
//忽略管道信号，发送数据给已关闭的socket
	
void timer1(int sock,short which,void *arg){
	event *ev =(event *)arg;
	cout<<"【hanxiao】"<<flush;
	if(!evtimer_pending(ev,&t1)){
		evtimer_del(ev);
		evtimer_add(ev,&t1);
	}
}

void timer2(int sock,short which,void *arg){
	
	cout<<"【hanxiao2】"<<flush;
	
}

void timer3(int sock,short which,void *arg){
	
	cout<<"【hanxiao3】"<<flush;
	
}
int main(int argc, char const *argv[])
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
	event_base *base = event_base_new();

	//定时器
	cout<<"timer test"<<endl;

	//event new普通的定时器
	event *ev1 =evtimer_new(base,timer1,event_self_cbarg());//上下文，定时器，回调函数，
	if(!ev1){
		cout<<"evtimer-new fail!"<<endl;
		return -1;
	}

	evtimer_add(ev1,&t1);

	static timeval t2;
	t2.tv_sec =1;
	t2.tv_usec =20000;
	event *ev2 = event_new(base,-1,EV_PERSIST,timer2,0);
	event_add(ev2,&t2);

	event *ev3 = event_new(base,-1,EV_PERSIST,timer3,0);
	//定时器优化：默认event用二叉堆存储（完全二叉树） 插入删除O(logn)
		// 优化到双向队列，插入删除 O(1)
	static timeval t3_in={3,0};
	const timeval *t3;
	t3 = event_base_init_common_timeout(base,&t3_in);
	event_add(ev3,t3);

	event_base_dispatch(base);
	event_base_free(base);
	return 0;
}
	
