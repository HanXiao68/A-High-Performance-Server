#include<iostream>
#include<event2/event.h>
#include<signal.h>
#include<thread>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
using namespace std;


void http_cb(struct evhttp_request *request,void *arg){
	cout<<"http_cb callback"<<endl;
	//1.获取浏览器的请求信息
	//uri
	const char *uri = evhttp_request_get_uri(request);
	cout<<"uri"<<uri<<endl;

	//请求类型 get post
	string cmdType;
	switch(evhttp_request_get_command(request)){
		case EVHTTP_REQ_GET:
			cmdType ="GET";
			break;
		case EVHTTP_REQ_POST:
			cmdType ="POST";
			break;
	}
	cout<<cmdType<<endl;

	// 消息报头(queue)

	// 设置支持图片，js css  下载zip文件
	// 获取文件的后缀名
	int pos = filepath.rfind('.');

	string postfix =filepath.substr(pos+1,filepath.size() -(pos+1));
	if(postfix =="jpg"||postfix =="pcd"||postfix =="png"||postfix =="css"){
		string tmp = "image/"+postfix;
		evhttp_add_header(outhead,"Content-Type", tmp.c_str());
	}else if (postfix == "html")
	{
		evhttp_add_header(outhead, "Content-Type", "text/html;charset=UTF8");
	}

	evkeyvalueq *header = evhttp_request_get_input_headers(request);
	cout<<"===========headers========="<<endl;
	//打印报头
	for(evkeyval* p=headers->tqh_first;p!=NULL;p =p->next.tqe_next){
		cout<<p->key<<" :"<<p->value<<endl;
	}

	// 请求正文(get 为空，post有表单信息)
	evbuffer *inbuffer = evhttp_request_get_input_buffer(request);
	char buf[1024] = {0};
	cout<<"===========input data"=========<<endl;
	while(evbuffer_get_length(inbuf)){
		int n =evbuffer_remove(inbuf,buf,sizeof(buf) -1);
		if(n>0){
			buf[n] ='\0';
			cout<<buf<<endl;
		}
	}

	//2.回复浏览器
	//设置文件路径
	string filepath = WEBROOT;
	filepath += uri;
	if (strcmp(uri, "/") == 0)
	{
		//默认加入首页文件
		filepath += DEFAULTINDEX;
	}


	//outbuf中装载回复的信息。

	//读取文件，放到buffer。再给evhttp_send_reply（）发送出去
	FILE *fp = fopen(filepath.c_str(),"rb");

	if(!fp){
		evhttp_send_reply(request,HTTP_NOTFOUND,"",0);
		return;//打开失败，给0 return
	}

	//状态行，消息报头，响应正文
	evbuffer *outbuf = evhttp_request_get_output_buffer(request);
	for(;;){
		int len =fread(buf,1,sizeof(buf),fp);
		if(len<=0) break;
		evbuffer_add(outbuf,buf,len);//直到读完buffer，再break
	}
	fclose(fp);

	evhttp_send_reply(request，HTTP_OK,"",outbuf);//对哪个消息回应，状态码，原因，具体buffer

}
int main(int argc, char const *argv[])
{
//忽略管道信号，发送数据给已关闭的socket
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
	// 创建libevent上下文
	event_base *base = event_base_new();
	if(base){// !=0
		cout<<"faild"<<endl;
	}
	//http服务器
	//1 创建evhttp上下文
	evhttp *evh = evhttp_new(base);

	//2 绑定端口，ip
	evhttp_bind_socket(evh,"0.0.0.0",8080);//上下文， ip，端口

	//3.设定回调函数
	evhttp_set_gencb(evh,http_cb,0);

	//进入事件主循环
	if(base)
	event_base_dispatch(base);
	if(base)
	event_base_free(base);
	if(evh)
	return 0;
}
	
