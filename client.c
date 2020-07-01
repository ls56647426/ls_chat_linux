/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
 *    Description: 	聊天程序
 * 					需求：UDP实现或者TCP实现
 * 						1、实现群聊，私聊，在线用户
 * 						2、增加管理员功能：实现禁言，踢出聊天程序
 * 						3，增加文件分享：文件传输
 * 						4，增加目录传输或者目录同步功能
 * 						5，远程协助功能（类似ssh，远程登录到对方主机，执行命令等功能）
 *
 *        Version:  1.0
 *        Created:  2020年05月24日 09时40分05秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  longshao (ls), ls56647426@foxmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

/* c std lib */
#include <stdio.h>
#include <string.h>

/* unix lib */
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>

/* third-party lib */
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <ls/wrap.h>
#include <ls/dir.h>

/* custom lib */
#include "packet.h"
#include "menu.h"

/* Menu class */
#define MENU_INIT 					0 		/* 主页菜单 */
#define MENU_LOGIN 					1 		/* 登录菜单 */
#define MENU_REGISTER 				2 		/* 注册菜单 */

#define MENU_HOME 					3 		/* 主页界面 */
#define MENU_CHATGROUP 				4 		/* 群聊界面 */
#define MENU_CHATUSER 				5 		/* 私聊界面 */
#define MENU_CREATE_GROUP 			6 		/* 创建群 */
#define MENU_ADD_USER 				7 		/* 添加用户 */
#define MENU_ADD_GROUP 				8 		/* 添加群 */
#define MENU_SELECT_CHATUSER 		9 		/* 选择私聊对象 */
#define MENU_SELECT_CHATGROUP 		10 		/* 选择群聊对象 */
#define MENU_CHATGROUP_EDIT 		11 		/* 群聊编辑状态 */
#define MENU_CHATUSER_EDIT 			12 		/* 私聊编辑状态 */

#define stacksize 0x100000

/* declare global variable */
static const int SERV_PORT = 5664;
static const char SERV_IP[INET_ADDRSTRLEN] = "192.168.31.104";
static int curMenu;
static User me;
static User you;
static Group other;
static LinkList *user_list;
static LinkTable *user_table;
static LinkList *group_user_list;
static LinkTable *group_user_table;
static pthread_attr_t attr;
static int tm = 0;

/* declare functions */
void read_cb(struct bufferevent *bev, void *ctx);
void write_cb(struct bufferevent *bev, void *ctx);
void event_cb(struct bufferevent *bev, short what, void *ctx);
void cmd_msg_cb(int fd, short events, void *arg);
void *file_upload(void *arg);
void *file_download(void *arg);

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  main
 *  Description:  
 * =====================================================================================
 */
int main(int argc, char *argv[])
{
	/* 列表初始化 */
	user_list = Create_LinkList();
	user_table = Create_LinkTable();
	group_user_list = Create_LinkList();
	group_user_table = Create_LinkTable();

	/* 当前状态初始化 */
	me.name[0] = 0;
	you.name[0] = 0;
	other.name[0] = 0;

	//创建base
	struct event_base *base;
	base = event_base_new();

	int fd = socket(AF_INET, SOCK_STREAM, 0);

	//创建bufferevent,并将通信的fd放到bufferevent中
	struct bufferevent *bev = NULL;
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	/* 初始化服务器端地址结构 */
	struct sockaddr_in serv_addr = { 0 };
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr.s_addr);
	serv_addr.sin_port = htons(SERV_PORT);

	//连接服务器
	bufferevent_socket_connect(bev, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	/* 监听、添加终端输入事件 */
	struct event *ev_cmd = event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST,
			cmd_msg_cb, bev);
	event_add(ev_cmd, NULL);

	/* 注册普通的信号事件 */
/*	struct event *esc_signal_event = evsignal_new(base, SIGINT, signal_cb, base);
	if(!signal_event || event_add(signal_event, NULL) < 0)
	{
		fprintf(stderr, "信号事件注册失败!\n");
		exit(1);
	}*/

	//设置回调函数
	bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
	bufferevent_enable(bev, EV_READ | EV_WRITE);

	event_base_dispatch(base);

	event_free(ev_cmd);
	event_base_free(base);

	return EXIT_SUCCESS;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_cb
 *  Description:  
 * =====================================================================================
 */
void read_cb(struct bufferevent *bev, void *ctx)
{
	//取得输入、输出缓冲区
//	struct evbuffer* input = bufferevent_get_input(bev);
	struct evbuffer* output = bufferevent_get_output(bev);
	Msg msg;
	UserNode *user;
	time_t t;
	struct tm *lt;

	/* 判断是否有线程锁 */
	if(tm != 0)
		return;

	/* 获取当前时间 */
	time(&t);
	lt = localtime(&t);

	/* 接收客户端返回的信息 */
	bufferevent_read(bev, &msg, sizeof(msg));
	switch(msg.type)
	{
		case MSG_REGISTER: 				/* 提示我注册成功 */
			printf("注册成功！\n");

			/* 提示主页信息 */
			showInitMenu();
			curMenu = MENU_INIT;
			break;
		case MSG_LOGIN: 				/* 提示别人登录 */
			if(!strcmp(msg.src.name, me.name))
			{
				/* 自己上线了 -- 登录成功 */
				me = msg.src;
				ls_ioctl(BOLD);
				printf("登录成功！\n");
				ls_ioctl(NONE);
				printf("\n");
				showHomeMenu();
				curMenu = MENU_HOME;
				break;
			}
			ls_ioctl(BLUE);
			printf("%02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
			ls_ioctl(NONE);
			ls_ioctl(BOLD);
			printf("'%s'上线了！\n", msg.src.name);
			ls_ioctl(NONE);
			break;
		case MSG_SENDMSG: 				/* 接收别人发送的消息 */
			ls_ioctl(BLUE);
			printf("'%s':'%s'(%d) %02d:%02d:%02d\n", msg.group.name, msg.src.name,
					msg.src.id, lt->tm_hour, lt->tm_min, lt->tm_sec);
			ls_ioctl(NONE);
			printf("%s\n", msg.info.info);
			break;
		case MSG_LOGOUT: 				/* 提示别人下线 */
			if(!strcmp(msg.src.name, me.name))
			{
				/* 自己上线了 -- 退出成功 */
				ls_ioctl(BOLD);
				printf("退出成功！\n");
				ls_ioctl(NONE);

				/* 提示主页信息 */
				showInitMenu();
				curMenu = MENU_INIT;
				break;
			}
			ls_ioctl(BLUE);
			printf("%02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
			ls_ioctl(NONE);
			ls_ioctl(BOLD);
			printf("'%s'下线了！\n", msg.src.name);
			ls_ioctl(NONE);
			break;

		case COMMAND_GET_ONLINEUSER: 	/* 获取在线列表 */
			/* 获取完成 */
			if(msg.info.info[0] == 0)
			{
				/* 以表格的形式回显 */
				Clear_LinkTable(user_table);

				LinkList_To_LinkTable_Auto(user_list, user_table,
						sizeof(UserNode), Get_UserNode_Name);

				for(LinkNode *p = user_table->rind_list->head.next;
						p != &user_table->rind_list->tail; p = p->next)
					((LinkIndent*)p)->indent.indent = LEFT_INDENT;

				printf("\n");
				Foreach_LinkTable(user_table, Print_LinkTableItem);

				showHomeMenu();
				curMenu = MENU_HOME;
				break;
			}

			/* 添加在线列表 */
			user = (UserNode *)malloc(sizeof(UserNode));
			user->user = msg.dest;
			Insert_LinkList_Pos(user_list, 1, (LinkNode *)user);

			/* 通知服务器端，发送下一个数据 */
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_CREATE_GROUP: 		/* 创建群 */
			ls_ioctl(BOLD);
			printf("创建群'%s'成功。\n", msg.group.name);
			ls_ioctl(NONE);
			showHomeMenu();
			curMenu = MENU_HOME;
			break;
		case COMMAND_ADD_USER: 			/* 加好友 */
			if(!strcmp(msg.src.name, me.name))
			{
				/* 加好友操作成功 */
				ls_ioctl(BOLD);
				printf("加'%s'为好友成功。\n", msg.dest.name);
				ls_ioctl(NONE);
				showHomeMenu();
				curMenu = MENU_HOME;
				break;
			}

			ls_ioctl(BOLD);
			printf("'%s'添加你为好友。\n", msg.src.name);
			ls_ioctl(NONE);
			break;
		case COMMAND_APPLY_ADD_GROUP: 		/* 申请加群 */
			ls_ioctl(BOLD);
			printf("加群成功。\n");
			ls_ioctl(NONE);
			showHomeMenu();
			curMenu = MENU_HOME;
			break;
		case COMMAND_INTO_CHATUSER: 	/* 进入私聊 */
			if(!strcmp(msg.src.name, me.name))
			{
				you = msg.dest;
				ls_ioctl(BOLD);
				printf("你开始与'%s'私聊。\n", msg.dest.name);
				ls_ioctl(NONE);
				showChatUserMenu();
				curMenu = MENU_CHATUSER;
				break;
			}

			ls_ioctl(BOLD);
			printf("'%s'开始与您私聊。\n", msg.src.name);
			ls_ioctl(NONE);
			break;
		case COMMAND_INTO_GROUP: 		/* 进群 */
			/* 自己进群 */
			if(!strcmp(me.name, msg.src.name))
			{
				other = msg.group;
				ls_ioctl(BOLD);
				printf("你进入了群聊。\n");
				ls_ioctl(NONE);
				showChatGroupMenu();
				curMenu = MENU_CHATGROUP;
				break;
			}

			ls_ioctl(BLUE);
			printf("%02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
			ls_ioctl(NONE);
			ls_ioctl(BOLD);
			printf("'%s'进入了群聊'%s'！\n", msg.src.name, msg.group.name);
			ls_ioctl(NONE);
			break;
		case COMMAND_GET_GROUP_ONLINEUSER: 		/* 获取群在线列表 */
			/* 获取完成 */
			if(msg.info.info[0] == 0)
			{
				/* 以表格的形式回显 */
				Clear_LinkTable(group_user_table);

				LinkList_To_LinkTable_Auto(group_user_list, group_user_table,
						sizeof(UserNode), Get_UserNode_Name);

				for(LinkNode *p = group_user_table->rind_list->head.next;
						p != &group_user_table->rind_list->tail; p = p->next)
					((LinkIndent*)p)->indent.indent = LEFT_INDENT;

				printf("\n");
				Foreach_LinkTable(group_user_table, Print_LinkTableItem);

				showChatGroupMenu();
				curMenu = MENU_CHATGROUP;
				break;
			}

			/* 添加群成员在线列表 */
			user = (UserNode *)malloc(sizeof(UserNode));
			user->user = msg.dest;
			Insert_LinkList_Pos(group_user_list, 1, (LinkNode *)user);

			/* 通知服务器端，发送下一个数据 */
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_GET_GROUP_FILE: 	/* 查看群文件 */
			/* 获取完成 */
			if(msg.info.file_info.type == 0)
			{
				ls_ioctl(BOLD);
				printf("群文件显示完毕。\n");
				ls_ioctl(NONE);
				showChatGroupMenu();
				break;
			}

			/* 显示群文件信息 */
			ls_ioctl(BLUE);
			printf("file:{name = '%s', size = %dB}\n", msg.info.file_info.name, msg.info.file_info.size);
			ls_ioctl(NONE);

			/* 通知服务器端，发送下一个数据 */
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_EXIT_CHATGROUP: 	/* 离开群聊 - 不是退群 */
			if(!strcmp(msg.src.name, me.name))
			{
				ls_ioctl(BOLD);
				printf("你离开了群聊。\n");
				ls_ioctl(NONE);
				/* 返回个人主界面菜单 */
				showHomeMenu();
				curMenu = MENU_HOME;
				break;
			}
			ls_ioctl(BLUE);
			printf("%02d:%02d:%02d\n", lt->tm_hour, lt->tm_min, lt->tm_sec);
			ls_ioctl(NONE);
			ls_ioctl(BOLD);
			printf("'%s'离开了群聊'%s'！\n", msg.src.name, msg.group.name);
			ls_ioctl(NONE);
			break;
			
		case ERRNO_SUCCESS: 			/* 操作成功 */
			printf("操作成功！\n");
			break;
		case ERRNO_ALREADYEXIST: 		/* 注册失败，当前用户已存在 */
			printf("错误：注册失败，当前用户已存在！\n");
			showInitMenu();
			curMenu = MENU_INIT;
			break;
		case ERRNO_INEXISTENCE: 		/* 登录失败，该用户不存在 */
			printf("错误：登录失败，该用户不存在！\n");
			showInitMenu();
			curMenu = MENU_INIT;
			break;
		case ERRNO_BEOFFLINE: 			/* 被挤掉 */
			printf("提示：该用户已在其他地点登录！\n");
			showInitMenu();
			curMenu = MENU_INIT;
			break;
		case ERRNO_GROUP_ALREADYEXIST: 	/* 群已存在 */
			printf("错误：群已经存在！\n");
			showHomeMenu();
			curMenu = MENU_HOME;
			break;
		case ERRNO_GROUP_INEXISTENCE: 	/* 群不存在 */
			printf("错误：群不存在！\n");
			showHomeMenu();
			curMenu = MENU_HOME;
			break;
		case ERRNO_ADDUSER_NOTFOUND: 	/* 加好友，没找到该用户 */
			printf("错误：没找到该用户！\n");
			showHomeMenu();
			curMenu = MENU_HOME;
			break;
		default:
			break;
	}
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  write_cb
 *  Description:  
 * =====================================================================================
 */
void write_cb(struct bufferevent *bev, void *ctx)
{
	//	printf("数据发送完成！\n");
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  event_cb
 *  Description:  
 * =====================================================================================
 */
void event_cb(struct bufferevent *bev,short what, void *ctx)
{
	if (what & BEV_EVENT_EOF) //遇到文件结束指示
	{
		printf("连接关闭。\n");
	}
	else if (what & BEV_EVENT_ERROR) //操作发生错误
	{
		perror("连接发生错误！\n");
	}
	else if (what & BEV_EVENT_TIMEOUT) //超时
	{
		printf("与服务器连接超时。。。\n");
	}
	else if (what & BEV_EVENT_CONNECTED) //连接已经完成
	{
		/* 提示主页信息 */
		showInitMenu();
		curMenu = MENU_INIT;
		return;
	}
	/* None of the other events can happen here, since we haven't enabled
	 * timeouts */


	bufferevent_free(bev);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  cmd_msg_cb
 *  Description:  
 * =====================================================================================
 */
void cmd_msg_cb(int fd, short events, void *arg)
{
	struct bufferevent *bev = (struct bufferevent *)arg;
	struct evbuffer* output = bufferevent_get_output(bev);
	char cmd_msg[BUFSIZ];
	int num, ret;
	Msg msg;
	TranFileThread *p;

	/* 判断是否有线程锁 */
	if(tm != 0)
		return;

	/** 
	 * 前端 
	 * 获取用户操作
	 */
	memset(cmd_msg, 0, BUFSIZ);
	ret = Read(fd, cmd_msg, sizeof(cmd_msg));
	cmd_msg[ret - 1] = 0; 		/* 去掉回车 */

	switch(curMenu)
	{
		case MENU_INIT: 		/* 初始菜单 */
			sscanf(cmd_msg, "%d", &num);
			switch(num)
			{
				case 1: 		/* 登录 */
					showLoginMenu();
					curMenu = MENU_LOGIN;
					break;
				case 2: 		/* 注册 */
					showRegisterMenu();
					curMenu = MENU_REGISTER;
					break;
				case 0: 		/* 退出 */
					bufferevent_free(bev);
					exit(1);
					break;
				default:
					break;
			}
			break;
		case MENU_LOGIN: 		/* 登录界面 */
			/* 获取当前登录用户信息 */
			memcpy(me.name, cmd_msg, USER_NAME_LEN - 1);
			me.name[USER_NAME_LEN - 1] = 0;

			/* 向服务器发送登录消息 */
			msg.type = MSG_LOGIN;
			msg.src = me;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_REGISTER: 	/* 注册界面 */
			/* 向服务器发送注册消息 */
			msg.type = MSG_REGISTER;
			memcpy(msg.src.name, cmd_msg, USER_NAME_LEN - 1);
			msg.src.name[USER_NAME_LEN - 1] = 0;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_HOME: 		/* 主页界面 */
			sscanf(cmd_msg, "%d", &num);

			switch(num)
			{
				case 1: 		/* 获取好友在线列表 */
					/* 清空当前的在线列表 */
					Clear_LinkList(user_list);

					/* 发送消息请求 */
					msg.type = COMMAND_GET_ONLINEUSER;
					msg.src = me;
					msg.info.info[0] = 0;
					evbuffer_add(output, &msg, sizeof(msg));
					break;
				case 2: 		/* 创建群 */
					showCreateGroupMenu();
					curMenu = MENU_CREATE_GROUP;
					break;
				case 3: 		/* 加好友 */
					showAddUserMenu();
					curMenu = MENU_ADD_USER;
					break;
				case 4: 		/* 加群 */
					showAddGroupMenu();
					curMenu = MENU_ADD_GROUP;
					break;
				case 5: 		/* 私聊 */
					/* 选择聊天对象 */
					showSelectChatUserMenu();
					curMenu = MENU_SELECT_CHATUSER;
					break;
				case 6: 		/* 进群 */
					showSelectChatGroupMenu();
					curMenu = MENU_SELECT_CHATGROUP;
					break;
				case 0: 		/* 退出登录 */
					msg.type = MSG_LOGOUT;
					msg.src = me;
					evbuffer_add(output, &msg, sizeof(msg));
					break;
				default:
					break;
			}
			break;
		case MENU_CREATE_GROUP: 		/* 创建群 */
			memcpy(msg.group.name, cmd_msg, GROUP_NAME_LEN - 1);
			msg.group.name[GROUP_NAME_LEN - 1] = 0;
			msg.type = COMMAND_CREATE_GROUP;
			msg.src = me;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_ADD_USER: 			/* 加好友 */
			msg.type = COMMAND_ADD_USER;
			msg.src = me;
			memcpy(msg.dest.name, cmd_msg, USER_NAME_LEN - 1);
			msg.dest.name[USER_NAME_LEN - 1] = 0;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_ADD_GROUP: 			/* 加群 */
			memcpy(msg.group.name, cmd_msg, GROUP_NAME_LEN - 1);
			msg.group.name[GROUP_NAME_LEN - 1] = 0;
			msg.type = COMMAND_APPLY_ADD_GROUP;
			msg.src = me;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_SELECT_CHATUSER: 		/* 选择私聊对象 */
			memcpy(you.name, cmd_msg, USER_NAME_LEN - 1);
			you.name[USER_NAME_LEN - 1] = 0;

			msg.type = COMMAND_INTO_CHATUSER;
			msg.group.name[0] = 0;
			msg.src = me;
			msg.dest = you;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_CHATUSER: 	/* 私聊 */
			sscanf(cmd_msg, "%d", &num);
			switch(num)
			{
				case 1: 		/* 发言 */
					curMenu = MENU_CHATUSER_EDIT;
					break;
				case 2: 		/* 控制对方电脑 */
					break;
				case 3: 		/* 请求远程协助 */
					break;
				case 0: 		/* 退出私聊 */
					showHomeMenu();
					curMenu = MENU_HOME;
					break;
				default:
					break;
			}
			break;
		case MENU_CHATUSER_EDIT: 		/* 私聊编辑状态 */
			memcpy(msg.info.info, cmd_msg, MSG_INFO_LEN - 1);
			msg.info.info[MSG_INFO_LEN - 1] = 0;

			ls_ioctl(UP);
			ls_ioctl(CLRLINE);

			/* 按Esc键退出编辑状态 */
			if(!strcmp(msg.info.info, ESC))
			{
				showChatUserMenu();
				curMenu = MENU_CHATUSER;
				break;
			}

			/* 向服务器发送聊天信息 */
			msg.type = MSG_SENDMSG;
			msg.group.name[0] = 0;
			msg.src = me;
			msg.dest = you;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_SELECT_CHATGROUP: 		/* 进群 - 选择群聊对象 */
			memcpy(other.name, cmd_msg, GROUP_NAME_LEN - 1);
			other.name[GROUP_NAME_LEN - 1] = 0;

			msg.type = COMMAND_INTO_GROUP;
			msg.group = other;
			msg.src = me;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MENU_CHATGROUP: 	/* 群聊 */
			sscanf(cmd_msg, "%d", &num);
			switch(num)
			{
				case 1: 		/* 获取群成员在线列表 */
					/* 清空上一次群成员在线列表 */
					Clear_LinkList(group_user_list);

					/* 发送消息申请 */
					msg.type = COMMAND_GET_GROUP_ONLINEUSER;
					msg.group = other;
					msg.src = me;
					evbuffer_add(output, &msg, sizeof(msg));
					break;
				case 2: 		/* 上传群文件 */
					/* 子线程传输文件传参初始化 */
					p = (TranFileThread *)malloc(sizeof(TranFileThread));
					p->bev = bev;
					tm = 1;

					/* 创建子线程 */
					pthread_create(&p->tid, &attr, file_upload, (void *)p);

					/* 线程分离 */
					pthread_detach(p->tid);
					break;
				case 3: 		/* 查看群文件 */
					msg.type = COMMAND_GET_GROUP_FILE;
					msg.group = other;
					msg.src = me;
					msg.info.file_info.type = 0;
					evbuffer_add(output, &msg, sizeof(msg));
					break;
				case 4: 		/* 下载群文件 */
					/* 子线程传输文件传参初始化 */
					p = (TranFileThread *)malloc(sizeof(TranFileThread));
					p->bev = bev;
					tm = 1;

					/* 创建子线程 */
					pthread_create(&p->tid, &attr, file_download, (void *)p);

					/* 线程分离 */
					pthread_detach(p->tid);
					break;
				case 5: 		/* 发言 */
					/* 进入聊天编辑状态 */
					curMenu = MENU_CHATGROUP_EDIT;
					break;
				case 0: 		/* 退出群聊 */
					msg.type = COMMAND_EXIT_CHATGROUP;
					msg.group = other;
					msg.src = me;
					evbuffer_add(output, &msg, sizeof(msg));
					break;
				default:
					break;
			}
			break;
		case MENU_CHATGROUP_EDIT: 		/* 编辑群聊中 */
			memcpy(msg.info.info, cmd_msg, MSG_INFO_LEN - 1);
			msg.info.info[MSG_INFO_LEN - 1] = 0;

			ls_ioctl(UP);
			ls_ioctl(CLRLINE);

			/* 按Esc键退出聊天模式 */
			if(!strcmp(cmd_msg, ESC))
			{
				printf("你退出了聊天。\n");
				showChatGroupMenu();
				curMenu = MENU_CHATGROUP;
				break;
			}

			/* 向服务器发送聊天信息 */
			msg.type = MSG_SENDMSG;
			msg.group = other;
			msg.src = me;
			msg.dest = you;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		default:
			break;
	}
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  file_upload
 *  Description:  
 * =====================================================================================
 */
void *file_upload(void *arg)
{
	TranFileThread *tft = (TranFileThread *)arg;
	FILE *fp;
	char buf[BUFSIZ];
	struct evbuffer *output = bufferevent_get_output(tft->bev);
	struct stat info;
	int cnt;
	Dir *pDir;
	LinkTable *table;

	showFileUploadMenu();

	for(;;)
	{
		/* 循环读取用户命令 */
		printf("select file> ");
		fflush(stdout);
		fflush(stdin);
		fgets(buf, BUFSIZ - 1, stdin);
		buf[BUFSIZ - 1] = 0;
		buf[strlen(buf) - 1] = 0;

		/* 选择该文件 */
		if(!strncmp(buf, "is ", 3))
		{
			memcpy(tft->msg.info.file_info.name, buf + 3, FILE_NAME_LEN - 1);
			tft->msg.info.file_info.name[FILE_NAME_LEN - 1] = 0;
			break;
		}

		if(!strncmp(buf, "cd ", 3))
		{
			chdir(buf + 3);
			continue;
		}

		if(!strncmp(buf, "ls ", 3))
		{
			pDir = Create_Dir();
			pDir->info_mode = DIR_LS;
			getAllDirFile(pDir, buf + 3, 0);
			Sort_LinkList(pDir->di_list, Compare_DirInfo);
			table = Dir_To_LinkTable(pDir, Get_DirInfo_Name);
			Foreach_LinkTable(table, Print_LinkTableItem);
			free(table);
			free(pDir);
			continue;
		}

		printf("想p吃呢，我不是shell，就支持上面三个命令！\n");
	}

	/* 获取文件信息 */
	lstat(tft->msg.info.file_info.name, &info);

	/* 发送开始标志 */
	tft->msg.type = COMMAND_FILE_UPLOAD;
	tft->msg.group = other;
	tft->msg.src = me;
	tft->msg.info.file_info.type = FILE_UPLOAD_HEAD;
	tft->msg.info.file_info.size = info.st_size;
	evbuffer_add(output, &tft->msg, sizeof(tft->msg));

	/* 打开要上传的文件 */
	fp = fopen(tft->msg.info.file_info.name, "r");
	if(!fp)
		perr_exit("fopen error");

	/* 分cnt次传输 */
	cnt = info.st_size / FILE_ONCE_SIZE + 1;
	tft->msg.info.file_data.type = FILE_UPLOAD_ING;
	for(int i = 0; i < cnt; i++)
	{
		tft->msg.info.file_data.offset = i * FILE_ONCE_SIZE;
		if(i + 1 == cnt) 		/* 最后一次，传输剩余的大小 */
			tft->msg.info.file_data.len = info.st_size % FILE_ONCE_SIZE;
		else 					/* 每次，固定传输大小 */
			tft->msg.info.file_data.len = FILE_ONCE_SIZE;

		fread(tft->msg.info.file_data.buf, sizeof(char), tft->msg.info.file_data.len,
				fp);
		evbuffer_add(output, &tft->msg, sizeof(tft->msg));
	}
	fclose(fp);

	/* 发送结束标志 */
	tft->msg.info.file_data.type = FILE_UPLOAD_END;
	evbuffer_add(output, &tft->msg, sizeof(tft->msg));

	free(tft);

	printf("文件上传成功！\n");
	showChatGroupMenu();
	tm = 0;
	curMenu = MENU_CHATGROUP;
	return NULL;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  file_download
 *  Description:  
 * =====================================================================================
 */
void *file_download(void *arg)
{
	TranFileThread *tft = (TranFileThread *)arg;
	int cnt;
	char path[PATH_MAX], buf[BUFSIZ];
	FILE *fp;
	Dir *pDir;
	LinkTable *table;
	struct evbuffer *output = bufferevent_get_output(tft->bev);

	/* 想要下载哪个文件 */
	printf("请输入要下载的文件名：\n");
	fgets(tft->msg.info.file_info.name, FILE_NAME_LEN - 1, stdin);
	tft->msg.info.file_info.name[FILE_NAME_LEN - 1] = 0;
	tft->msg.info.file_info.name[strlen(tft->msg.info.file_info.name) - 1] = 0;

	/* 封装请求头 */
	tft->msg.type = COMMAND_FILE_DOWNLOAD;
	tft->msg.group = other;
	tft->msg.src = me;
	tft->msg.info.file_info.type = FILE_DOWNLOAD_HEAD;
	evbuffer_add(output, &tft->msg, sizeof(tft->msg));

	/* 接收群文件信息 */
	while(bufferevent_read(tft->bev, &tft->msg, sizeof(tft->msg)) <= 0);
	cnt = tft->msg.info.file_info.size / FILE_ONCE_SIZE + 1;

	/* 选择要下载到哪 */
	showFileDownloadMenu();

	for(;;)
	{
		/* 循环读取用户命令 */
		printf("select path> ");
		fflush(stdout);
		fflush(stdin);
		fgets(buf, BUFSIZ - 1, stdin);
		buf[BUFSIZ - 1] = 0;
		buf[strlen(buf) - 1] = 0;

		/* 选择该文件 */
		if(!strncmp(buf, "is ", 3))
			break;

		if(!strncmp(buf, "cd ", 3))
		{
			chdir(buf + 3);
			continue;
		}

		if(!strncmp(buf, "ls ", 3))
		{
			pDir = Create_Dir();
			pDir->info_mode = DIR_LS;
			getAllDirFile(pDir, buf + 3, 0);
			Sort_LinkList(pDir->di_list, Compare_DirInfo);
			table = Dir_To_LinkTable(pDir, Get_DirInfo_Name);
			Foreach_LinkTable(table, Print_LinkTableItem);
			free(table);
			free(pDir);
			continue;
		}

		printf("想p吃呢，我不是shell，就支持上面三个命令！\n");
	}
	getcwd(path, PATH_MAX);
	if(path[strlen(path) - 1] != '/')
		strcat(path, "/");
	strncat(path, buf + 3, PATH_MAX - strlen(path) - 1);
	path[PATH_MAX - 1] = 0;
	fp = fopen(path, "w");
	if(!fp)
		perr_exit("fopen error");

	/* 分cnt次传输 */
	while(cnt--)
	{
		while(bufferevent_read(tft->bev, &tft->msg, sizeof(tft->msg)) <= 0);
		fwrite(tft->msg.info.file_data.buf, sizeof(char), tft->msg.info.file_data.len, fp);
	}
	fclose(fp);

	/* 最后，接收到一个结束标志 */
	while(bufferevent_read(tft->bev, &tft->msg, sizeof(tft->msg)) <= 0);

	tm = 0;
	free(tft);
	printf("下载成功。\n");
	showChatGroupMenu();
	curMenu = MENU_CHATGROUP;
	return NULL;
}




