/*
 * =====================================================================================
 *
 *       Filename:  server.c
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
 *        Created:  2020年05月24日 09时37分22秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  longshao (ls), ls56647426@foxmail.com
 *   Organization:  
 *   				2020-05-25 22:34：做到《申请加群》功能，修改了GroupUserMapNode结构体，
 *   					之后的一系列变化，明天再做。
 *  
 * 					2020-05-27 22:49：客户端的界面架构处理好了，明天改一下具体的menu.c内容。
 * 						界面都处理好之后，就方便实现接下来的功能了。
 *
 * 					2020-05-28 17:06：客户端界面的不同控件缓冲区，太麻烦了，要记录太多信
 * 						息了，短时间没法实现好，暂时先放弃吧。c本来就不是用来做视图的东西。
 *
 *   				2020-05-29 01:14：昨完下载群文件了，明天把查看群文件实现了。
 *
 *   				2020-05-29 12:52：前三个功能基本没bug了。
 *
 * =====================================================================================
 */

/* c std lib */
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* unix lib */
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
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

#define stacksize 0x100000
#define SERV_PORT 5664
#define SERV_IP "192.168.31.104"

/* declare global variable */
static int uid, gid, gumid, uumid;
static LinkList *user_list;
static LinkList *group_list;
static LinkList *group_user_map_list;
static LinkList *user_user_map_list;
static pthread_attr_t attr;
static LinkList *tm_list;
static Dir *pDir;

/* declare functions */
void read_data();
void read_cb(struct bufferevent *bev, void *arg);
void write_cb(struct bufferevent *bev, void *arg);
void event_cb(struct bufferevent *bev,short events, void *arg);
void listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
		struct sockaddr *addr, int len, void *ptr);
void signal_cb(evutil_socket_t, short, void *);
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
	/* 获取本地数据 */
	read_data();
	tm_list = Create_LinkList();

	/* 初始化子线程大小 */
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stacksize);

	//初始化服务器地址结构
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERV_IP, &serv_addr.sin_addr.s_addr);
	serv_addr.sin_port = htons(SERV_PORT);

	//创建event_base
	struct event_base *base;
	base = event_base_new();

	//创建监听器
	struct evconnlistener *listener;
	listener = evconnlistener_new_bind(base, listener_cb, base, 
			LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, 
			(struct sockaddr*)&serv_addr, sizeof(serv_addr));

	/* 注册普通的信号事件 */
	struct event *signal_event = evsignal_new(base, SIGINT, signal_cb, base);
	if(!signal_event || event_add(signal_event, NULL) < 0)
	{
		fprintf(stderr, "信号事件注册失败!\n");
		exit(1);
	}

	//启动循环监听
	event_base_dispatch(base);

	//释放操作
	evconnlistener_free(listener);
	event_base_free(base);

	return EXIT_SUCCESS;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_data
 *  Description:  
 * =====================================================================================
 */
void read_data()
{
	FILE *ufp, *gfp, *gumfp, *uumfp;

	/* 获取本地数据库信息 */
	/* 获取用户数据 */
	ufp = fopen("user.txt", "r+");
	if(!ufp)
	{
		perror("user.txt fopen error");
		exit(1);
	}
	
	user_list = Create_LinkList();
	UserNode *user;

	fscanf(ufp, "%d\n", &uid);
	while(!feof(ufp))
	{
		user = (UserNode*)malloc(sizeof(UserNode));
		fscanf(ufp, "%d,%s\n", &user->user.id, user->user.name);
		user->status = USER_STAT_OFFLINE;

		Insert_LinkList_Pos(user_list, 1, (LinkNode *)user);
	}
	fclose(ufp);
	
	/* 获取群数据 */
	gfp = fopen("group.txt", "r+");
	if(!gfp)
	{
		perror("group.txt fopen error");
		exit(1);
	}

	group_list = Create_LinkList();
	GroupNode *group;

	fscanf(gfp, "%d\n", &gid);
	while(!feof(gfp))
	{
		group = (GroupNode*)malloc(sizeof(GroupNode));
		fscanf(gfp, "%d,%s\n", &group->group.id, group->group.name);

		Insert_LinkList_Pos(group_list, 1, (LinkNode *)group);
	}
	fclose(gfp);

	/* 获取 用户-群 映射表信息 */
	gumfp = fopen("group_user_map.txt", "r+");
	if(!gumfp)
	{
		perror("group_user_map.txt fopen error");
		exit(1);
	}
	
	group_user_map_list = Create_LinkList();
	GroupUserMapNode *group_user_map;

	fscanf(gumfp, "%d\n", &gumid);
	while(!feof(gumfp))
	{
		group_user_map = (GroupUserMapNode*)malloc(sizeof(GroupUserMapNode));
		fscanf(gumfp, "%d,%d,%d,%d,%d\n", &group_user_map->id,
				&group_user_map->uid, &group_user_map->gid,
				&group_user_map->status, &group_user_map->permission);

		Insert_LinkList_Pos(group_user_map_list, 1, (LinkNode *)group_user_map);
	}
	fclose(gumfp);
	
	/* 获取 用户 - 用户 映射表 数据 */
	uumfp = fopen("user_user_map.txt", "r+");
	if(!uumfp)
	{
		perror("user_user_map.txt fopen error");
		exit(1);
	}
	
	user_user_map_list = Create_LinkList();
	UserUserMapNode *user_user_map;

	fscanf(uumfp, "%d\n", &uumid);
	while(!feof(ufp))
	{
		user_user_map = (UserUserMapNode *)malloc(sizeof(UserUserMapNode));
		fscanf(uumfp, "%d,%d,%d\n", &user_user_map->id, &user_user_map->uid1,
				&user_user_map->uid2);

		Insert_LinkList_Pos(user_user_map_list, 1, (LinkNode *)user_user_map);
	}
	fclose(uumfp);
	
	/* 获取本地文件数据 */
	/* ... */
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  write_data
 *  Description:  
 * =====================================================================================
 */
void write_data()
{
	FILE *ufp, *gfp, *gumfp, *uumfp;

	/* 获取本地数据库信息 */
	/* 获取用户数据 */
	ufp = fopen("user.txt", "r+");
	if(!ufp)
	{
		perror("user.txt fopen error");
		exit(1);
	}
	
	fprintf(ufp, "%d\n", uid);
	/* 从后往前写，因为读的时候是头插法 */
	for(LinkNode *p = user_list->tail.prev; p != &user_list->head; p = p->prev)
		fprintf(ufp, "%d,%s\n", ((UserNode *)p)->user.id, ((UserNode *)p)->user.name);
	fclose(ufp);
	
	/* 获取群数据 */
	gfp = fopen("group.txt", "r+");
	if(!gfp)
	{
		perror("group.txt fopen error");
		exit(1);
	}

	fprintf(gfp, "%d\n", gid);
	/* 从后往前写，因为读的时候是头插法 */
	for(LinkNode *p = group_list->tail.prev; p != &group_list->head; p = p->prev)
		fprintf(gfp, "%d,%s\n", ((GroupNode *)p)->group.id, ((GroupNode *)p)->group.name);
	fclose(gfp);

	/* 获取 用户-群 映射表信息 */
	gumfp = fopen("group_user_map.txt", "r+");
	if(!gumfp)
	{
		perror("group_user_map.txt fopen error");
		exit(1);
	}
	
	fprintf(gumfp, "%d\n", gumid);
	/* 从后往前写，因为读的时候是头插法 */
	for(LinkNode *p = group_user_map_list->tail.prev; p != &group_user_map_list->head;
			p = p->prev)
		fprintf(gumfp, "%d,%d,%d,%d,%d\n", ((GroupUserMapNode *)p)->id,
				((GroupUserMapNode *)p)->uid, ((GroupUserMapNode *)p)->gid,
				((GroupUserMapNode *)p)->status, ((GroupUserMapNode *)p)->permission);
	fclose(gumfp);
	
	/* 获取 用户-群 映射表信息 */
	uumfp = fopen("user_user_map.txt", "r+");
	if(!uumfp)
	{
		perror("user_user_map.txt fopen error");
		exit(1);
	}
	
	fprintf(uumfp, "%d\n", uumid);
	/* 从后往前写，因为读的时候是头插法 */
	for(LinkNode *p = user_user_map_list->tail.prev; p != &user_user_map_list->head;
			p = p->prev)
		fprintf(uumfp, "%d,%d,%d\n", ((UserUserMapNode *)p)->id,
				((UserUserMapNode *)p)->uid1, ((UserUserMapNode *)p)->uid2);
	fclose(uumfp);

	/* 获取本地文件数据 */
	/* ... */
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  listener_cb
 *  Description:  
 * =====================================================================================
 */
void listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
		struct sockaddr *addr, int len, void *ptr)
{
	struct event_base *base = (struct event_base*)ptr;
	char buf[256];

	struct sockaddr_in *clie_addr = (struct sockaddr_in *)addr;
	evutil_inet_ntop(clie_addr->sin_family, &clie_addr->sin_addr, buf, sizeof(buf));
	printf("新的连接%d, IP: %s\n", fd, buf);

	//添加新事件
	struct bufferevent* bev;
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	//给bufferevent缓冲区设置回调
	bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);

	//启动bufferevent的读缓冲区，默认是disable的.
	bufferevent_enable(bev, EV_READ);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  read_cb
 *  Description:  
 * =====================================================================================
 */
void read_cb(struct bufferevent *bev, void *arg)
{
	char buf[1024] = {0};
	int ret;
	Msg msg;
	UserNode *user, *user_tmp, *user_dest;
	GroupNode *group, *group_tmp;
	GroupUserMapNode *gum_node, *gum_node_tmp;
	UserUserMapNode *uum_node, *uum_node_tmp;
	TranFileThread *p;
	ThreadMutexNode *tm_node;
	struct evbuffer* output = bufferevent_get_output(bev);

	/* 判断是否有线程锁 */
	tm_node = (ThreadMutexNode *)malloc(sizeof(ThreadMutexNode));
	tm_node->bev = bev;
	ret = Find_LinkList(tm_list, (LinkNode *)tm_node,
			Compare_ThreadMutexNode);
	free(tm_node);
	if(ret != -1)
		return;

	/* 获取客户端发来的信息 */
	bufferevent_read(bev, &msg, sizeof(msg));

	/* 服务器处理操作 */
	switch(msg.type)
	{
		case MSG_REGISTER:		/* 注册 */
			/* 检测是否有同一用户存在，如果有要提示回去 */
			user = (UserNode*)malloc(sizeof(UserNode));
			user->user = msg.src;
			ret = Find_LinkList(user_list, (LinkNode *)user, Compare_UserNode);
			if(ret != -1)
			{
				/* 当前用户名已存在 */
				msg.type = ERRNO_ALREADYEXIST;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 插入数据库 */
			user->user.id = ++uid;
			user->status = USER_STAT_OFFLINE;
			Insert_LinkList_Pos(user_list, 1, (LinkNode *)user);

			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case MSG_LOGIN: 		/* 登录 */
			/* 判断是否注册过 */
			user = (UserNode *)malloc(sizeof(UserNode));
			user->user = msg.src;
			ret = Find_LinkList(user_list, (LinkNode *)user, Compare_UserNode);
			free(user);
			if(ret == -1)
			{
				msg.type = ERRNO_INEXISTENCE;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 判断当前状态，是否已经登录(不能同时多地点登录) */
			user = (UserNode *)Find_LinkList_Pos(user_list, ret);

			/* 如果已经登录，则挤掉上一次登录地点 */
			if(user->status == USER_STAT_ONLINE)
			{
				/* 通知被挤掉 */
				msg.type = ERRNO_BEOFFLINE;
				bufferevent_write(user->bev, &msg, sizeof(msg));

				/* 给一个短暂的离线状态 */
				bufferevent_free(user->bev);
				user->status = USER_STAT_OFFLINE;
				
				/* 告诉所有人，我掉了 */
//				user_tmp = (UserNode *)malloc(sizeof(UserNode));
				msg.type = MSG_LOGOUT;
//				for(LinkNode *p = group_user_map_list->head.next;
//						p != &group_user_map_list->tail; p = p->next)
				/* 暂时直接发给所有用户，没有群 */
				for(LinkNode *p = user_list->head.next; p != &user_list->tail;
						p = p->next)
				{
					/* 获取下线的好友或在同一个群的所有人 */
//					user_tmp->user.id = ((GroupUserMapNode *)p)->uid;
//					user_dest = (UserNode *)Find_LinkList_Pos(user_list ,
//							Find_LinkList(user_list, p, Compare_UserNode_Id));

					/* 如果这个人不在线，跳过 */
					if(((UserNode *)p)->status == USER_STAT_OFFLINE)
						continue;

					bufferevent_write(((UserNode *)p)->bev, &msg, sizeof(msg));
				}
//				free(user_tmp);
			}

			/* 更新bufevent，改回在线状态 */
			user->bev = bev;
//			strcpy(user->user.login_ip, bev.ev_base->);
			user->status = USER_STAT_ONLINE;

/* 			下面提示所有人的时候，包括了我自己
 * 			msg.type = ERRNO_SUCCESS;
			evbuffer_add(output, &msg, sizeof(msg));*/
			
			/* 告诉所有人，我上线了 */
//			user_tmp = (UserNode *)malloc(sizeof(UserNode));
			msg.type = MSG_LOGIN;
			msg.src.id = user->user.id;
//			for(gum_node = (GroupUserMapNode *)group_user_map_list->head.next;
//					(LinkNode *)gum_node != &group_user-map_list->tail;
//					gum_node = (GroupUserMapNode *)gum_node->node.next)
			for(LinkNode *p = user_list->head.next; p != &user_list->tail;
					p = p->next)
			{
				/* 获取下线的好友或在同一个群的所有人 */
//				user_tmp->user.id = gum_node->uid;
//				user_dest = (UserNode *)Find_LinkList_Pos(user_list ,
//						Find_LinkList(user_list, (LinkNode *)user_tmp, Compare_UserNode_Id));

				/* 如果这个人不在线，跳过 */
				if(((UserNode *)p)->status == USER_STAT_OFFLINE)
					continue;

				bufferevent_write(((UserNode *)p)->bev, &msg, sizeof(msg));
			}
//			free(user_tmp);
			break;
		case MSG_SENDMSG: 		/* 发送消息 */
			/* 找到聊天所在群 */
			group = (GroupNode *)malloc(sizeof(GroupNode));
			group->group = msg.group;
			ret = Find_LinkList(group_list, (LinkNode *)group, Compare_GroupNode);
			
			/* 没有群，说明是私聊 */
			if(ret == -1)
			{
				/* 判断是否找到这个人 */
				user_tmp = (UserNode *)malloc(sizeof(UserNode));
				user_tmp->user = msg.dest;
				ret = Find_LinkList(user_list, (LinkNode *)user_tmp, Compare_UserNode);
				if(ret == -1)
				{
					msg.type = ERRNO_NOTFOUND;
					evbuffer_add(output, &msg, sizeof(msg));
					free(user_tmp);
					break;
				}
				free(user_tmp);

				/* 获取该用户系统状态 */
				user = (UserNode *)Find_LinkList_Pos(user_list, ret);
			
				/* 离线不发送消息 */
				if(user->status == USER_STAT_OFFLINE)
					break;

				/* 判断是否是好友 */
				uum_node_tmp = (UserUserMapNode *)malloc(sizeof(UserUserMapNode));
				uum_node_tmp->uid1 = msg.src.id;
				uum_node_tmp->uid2 = user->user.id;
				ret = Find_LinkList(user_user_map_list, (LinkNode *)uum_node_tmp,
						Compare_UserUserMapNode_Uid);
				if(ret == -1)
				{
					msg.type = ERRNO_NOTFOUND;
					evbuffer_add(output, &msg, sizeof(msg));
					free(user_tmp);
					free(group);
					break;
				}
				free(uum_node_tmp);
				free(group);

				/* 给对方发送消息 */
				bufferevent_write(user->bev, &msg, sizeof(msg));

				/* 给自己回传消息 */
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}
			free(group);

			/* 群聊 */
			group = (GroupNode *)Find_LinkList_Pos(group_list, ret);

			/* 判断用户是否在群内 */
			gum_node_tmp = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			gum_node_tmp->uid = msg.src.id;
			gum_node_tmp->gid = group->group.id;
			ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node_tmp,
						Compare_GroupUserMapNode_Uid_Gid);
			free(gum_node_tmp);
			if(ret == -1)
			{
				/* 不在的话，就说没找到这个群 */
				msg.type = ERRNO_GROUP_INEXISTENCE;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 给群里所有人发消息 */
			user_tmp = (UserNode *)malloc(sizeof(UserNode));
			for(LinkNode *p = group_user_map_list->head.next;
					p != &group_user_map_list->tail; p = p->next)
			{
				/* 找到群成员 */
				if(group->group.id != ((GroupUserMapNode *)p)->gid)
					continue;

				/* 获取在线的好友或在同一个群的所有人 */
				user_tmp->user.id = ((GroupUserMapNode *)p)->uid;
				user_dest = (UserNode *)Find_LinkList_Pos(user_list ,
						Find_LinkList(user_list, (LinkNode *)user_tmp, Compare_UserNode_Id));

				/* 如果这个人不在线，跳过 */
				if(user_dest->status == USER_STAT_OFFLINE)
					continue;

				bufferevent_write(user_dest->bev, &msg, sizeof(msg));
			}
			free(user_tmp);
			break;
		case MSG_LOGOUT: 		/* 退出登录 */
			/* 告诉所有人，我掉了 */
//			user_tmp = (UserNode *)malloc(sizeof(UserNode));
//			for(gum_node = (GroupUserMapNode *)group_user_map_list->head.next;
//					(LinkNode *)gum_node != &group_user-map_list->tail;
//					gum_node = (GroupUserMapNode *)gum_node->node.next)
			for(LinkNode *p = user_list->head.next; p != &user_list->tail;
					p = p->next)
			{
				/* 获取下线的好友或在同一个群的所有人 */
//				user_tmp->user.id = gum_node->uid;
//				user_dest = (UserNode *)Find_LinkList_Pos(user_list ,
//						Find_LinkList(user_list, p, Compare_UserNode_Id));

				/* 如果这个人不在线，跳过 */
				if(((UserNode *)p)->status == USER_STAT_OFFLINE)
					continue;

				bufferevent_write(((UserNode *)p)->bev, &msg, sizeof(msg));
			}
//			free(user_tmp);

			user_tmp = (UserNode *)malloc(sizeof(UserNode));
			user_tmp->user = msg.src;
			user = (UserNode *)Find_LinkList_Pos(user_list,
					Find_LinkList(user_list, (LinkNode *)user_tmp, Compare_UserNode));
			user->status = USER_STAT_OFFLINE;
			free(user_tmp);
//			bufferevent_free(bev);
			break;
		case COMMAND_GET_ONLINEUSER: 		/* 获取在线列表 */
			/* 第一次发送的数据包是1，接下来每次+1，结束再发个0 */
			msg.info.info[0]++;

			if(msg.info.info[0] > user_list->size + 1 || msg.info.info[0] < 1)
			{
				msg.info.info[0] = 0; 			/* 发送结束标志 */
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 循环遍历用户列表，直到找到在线的好友，发出去 */
			uum_node_tmp = (UserUserMapNode *)malloc(sizeof(UserUserMapNode));
			for(LinkNode *p = Find_LinkList_Pos(user_list, msg.info.info[0]); 
					p && msg.info.info[0] <= user_list->size; msg.info.info[0]++,
					p = p->next)
			{
				/* 判断是否是好友 */
				uum_node_tmp->uid1 = msg.src.id;
				uum_node_tmp->uid2 = ((UserNode *)p)->user.id;
				ret = Find_LinkList(user_user_map_list, (LinkNode *)uum_node_tmp,
						Compare_UserUserMapNode_Uid);
				if(ret == -1)
					continue;

				if(((UserNode *)p)->status != USER_STAT_ONLINE)
					continue;

				msg.dest = ((UserNode *)p)->user;
				evbuffer_add(output, &msg, sizeof(msg));
				free(uum_node_tmp);
				return;
			}

			/* 发送完成 */
			msg.info.info[0] = 0; 			/* 发送结束标志 */
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_CREATE_GROUP: 		/* 创建群 */
			/* 验证该群是否存在 */
			group = (GroupNode *)malloc(sizeof(GroupNode));
			strcpy(group->group.name, msg.group.name);
			ret = Find_LinkList(group_list, (LinkNode *)group, Compare_GroupNode);
			if(ret != -1)
			{
				/* 如果已存在，则提示 */
				msg.type = ERRNO_GROUP_ALREADYEXIST;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}
			
			/* 否则，创建该群 */
			group->group.id = ++gid;
			Insert_LinkList_Pos(group_list, 1, (LinkNode *)group);

			/* 建立 用户-群 映射 */
			gum_node = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			gum_node->id = ++gumid;
			gum_node->uid = msg.src.id;
			gum_node->gid = group->group.id;
			gum_node->status = GROUP_USER_STAT_NORMAL;
			gum_node->permission = PERM_ADMI;
			Insert_LinkList_Pos(group_user_map_list, 1, (LinkNode *)gum_node);

			/* 创建群文件夹 */
			chdir("group_file");
			sprintf(buf, "group_%d", group->group.id);
			mkdir(buf, 0644);
			chdir("..");

			/* 返回客户端成功通知 */
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_ADD_USER: 		/* 加好友 */
			/* 校验用户是否存在 */
			user_tmp = (UserNode *)malloc(sizeof(UserNode));
			user_tmp->user = msg.dest;
			ret = Find_LinkList(user_list, (LinkNode *)user_tmp, Compare_UserNode);
			free(user_tmp);
			if(ret == -1)
			{
				msg.type = ERRNO_ADDUSER_NOTFOUND;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 获取该用户数据 */
			user = (UserNode *)Find_LinkList_Pos(user_list, ret);

			/* 判断是否已经是好友，不是，则加好友，是，则直接返回成功 */
			uum_node = (UserUserMapNode *)malloc(sizeof(UserUserMapNode));
			uum_node->uid1 = msg.src.id;
			uum_node->uid2 = user->user.id;
			ret = Find_LinkList(user_user_map_list, (LinkNode *)uum_node,
					Compare_UserUserMapNode_Uid);
			if(ret == -1)
			{
				uum_node->id = ++uumid;
				Insert_LinkList_Pos(user_user_map_list, 1,
						(LinkNode *)uum_node);
			}
	
			/* 通知自己 */
			evbuffer_add(output, &msg, sizeof(msg));

			/* 通知对方 */
			if(user->status == USER_STAT_ONLINE)
				bufferevent_write(user->bev, &msg, sizeof(msg));
			break;
		case COMMAND_APPLY_ADD_GROUP: 		/* 申请加群 */
			/* 校验该群是否存在 */
			group = (GroupNode *)malloc(sizeof(GroupNode));
			group->group = msg.group;
			ret = Find_LinkList(group_list, (LinkNode *)group, Compare_GroupNode);
			if(ret == -1)
			{
				msg.type = ERRNO_GROUP_INEXISTENCE;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 获取该群详细信息 */
			group = (GroupNode *)Find_LinkList_Pos(group_list, ret);

			/* 判断用户是否已经在群内，不在，则加群，在，则直接返回成功 */
			gum_node = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			gum_node->uid = msg.src.id;
			gum_node->gid = group->group.id;
			ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node,
					Compare_GroupUserMapNode_Uid_Gid);
			if(ret == -1)
			{
				gum_node->id = ++gumid;
				gum_node->status = GROUP_USER_STAT_NORMAL;
				gum_node->permission = PERM_USER;
				Insert_LinkList_Pos(group_user_map_list, 1,
						(LinkNode *)gum_node);
			}

			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_INTO_CHATUSER: 	/* 进入私聊 */
			/* 判断私聊对象是否存在 */
			user_tmp = (UserNode *)malloc(sizeof(UserNode));
			user_tmp->user = msg.dest;
			ret = Find_LinkList(user_list, (LinkNode *)user_tmp,
					Compare_UserNode);
			if(ret == -1)
			{
				/* 没有找到对方，暂时用ADDUSER这个，一个效果 */
				msg.type = ERRNO_ADDUSER_NOTFOUND;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 获取对方详细数据 */
			user = (UserNode *)Find_LinkList_Pos(user_list, ret);

			/* 判断是否在线 */
			if(user->status != USER_STAT_OFFLINE)
				bufferevent_write(user->bev, &msg, sizeof(msg));

			/* 回显 */
			msg.dest = user->user;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_INTO_GROUP: 		/* 进群 */
			/* 判断该群是否存在 */
			group_tmp = (GroupNode *)malloc(sizeof(GroupNode));
			group_tmp->group = msg.group;
			ret = Find_LinkList(group_list, (LinkNode *)group_tmp,
					Compare_GroupNode);
			free(group_tmp);
			if(ret == -1)
			{
				msg.type = ERRNO_GROUP_INEXISTENCE;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 获取该群数据 */
			group = (GroupNode *)Find_LinkList_Pos(group_list, ret);

			/* 判断该用户是否是该群的群成员 */
			gum_node = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			gum_node->uid = msg.src.id;
			gum_node->gid = group->group.id;
			ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node,
					Compare_GroupUserMapNode_Uid_Gid);
			if(ret == -1)
			{
				/* 如果不是群成员，就提示没找到该群 */
				msg.type = ERRNO_GROUP_INEXISTENCE;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 提示所有在线群成员，该用户进群了 */
			msg.group = group->group;
			gum_node = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			for(LinkNode *p = user_list->head.next; p != &user_list->tail;
					p = p->next)
			{
				/* 判断是否在线 */
				if(((UserNode *)p)->status != USER_STAT_ONLINE)
					continue;

				/* 找到该群成员 */
				gum_node->uid = ((UserNode *)p)->user.id;
				gum_node->gid = group->group.id;
				ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node,
						Compare_GroupUserMapNode_Uid_Gid);
				if(ret == -1)
					continue;

				/* 发送通知 */
				bufferevent_write(((UserNode *)p)->bev, &msg, sizeof(msg));
			}
			break;
		case COMMAND_GET_GROUP_ONLINEUSER: 		/* 获取群内在线成员列表 */
			/* 第一次发送的数据包是1，接下来每次+1，结束再发个0 */
			msg.info.info[0]++;
			if(msg.info.info[0] > user_list->size + 1 || msg.info.info[0] < 1)
			{
				msg.info.info[0] = 0; 			/* 发送结束标志 */
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 发送群在线列表 */
			gum_node_tmp = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			for(LinkNode *p = Find_LinkList_Pos(user_list, msg.info.info[0]);
					p && msg.info.info[0] <= user_list->size; p = p->next,
					msg.info.info[0]++)
			{
				/* 判断是否在群里 */
				gum_node_tmp->uid = ((UserNode *)p)->user.id;
				gum_node_tmp->gid = msg.group.id;
				ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node_tmp,
						Compare_GroupUserMapNode_Uid_Gid);
				if(ret == -1)
					continue;

				/* 判断是否在线 */
				if(((UserNode *)p)->status != USER_STAT_ONLINE)
					continue;

				msg.dest = ((UserNode *)p)->user;
				free(gum_node_tmp);
				evbuffer_add(output, &msg, sizeof(msg));
				return;
			}

			/* 发送结束标志 */
			msg.info.info[0] = 0;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_FILE_UPLOAD: 		 /* 上传群文件 */
			/* 子线程传输文件传参初始化 */
			p = (TranFileThread *)malloc(sizeof(TranFileThread));
			p->msg = msg;
			p->bev = bev;
			tm_node = (ThreadMutexNode *)malloc(sizeof(ThreadMutexNode));
			tm_node->bev = bev;
			Insert_LinkList_Pos(tm_list, 1, (LinkNode *)tm_node);

			/* 创建子线程 */
			pthread_create(&p->tid, &attr, file_upload, (void *)p);

			/* 线程分离 */
			pthread_detach(p->tid);
			break;
		case COMMAND_GET_GROUP_FILE: 		/* 查看群文件 */
			msg.info.file_info.type++;

			/* 接收到申请头 */
			if(msg.info.file_info.type == 1)
			{
				/* 获取该群文件 */
				sprintf(buf, "group_file/group_%d", msg.group.id);
				pDir = Create_Dir();
				pDir->info_mode = DIR_LS_L;
				getAllDirFile(pDir, buf, 0);
				Sort_LinkList(pDir->di_list, Compare_DirInfo);
			}

			/* 超出范围，直接结束 */
			if(msg.info.file_info.type < 1 ||
					msg.info.file_info.type > pDir->di_list->size)
			{
				free(pDir);
				msg.info.file_info.type = 0;
				evbuffer_add(output, &msg, sizeof(msg));
				break;
			}

			/* 发送群文件信息 */
			for(LinkNode *p = Find_LinkList_Pos(pDir->di_list, msg.info.file_info.type);
					p && msg.info.file_info.type <= pDir->di_list->size; p = p->next,
					msg.info.file_info.type++)
			{
				strncpy(msg.info.file_info.name, ((Dir_Info *)p)->name, FILE_NAME_LEN);
				sscanf(((Dir_Info *)p)->size, "%d", &msg.info.file_info.size);
				evbuffer_add(output, &msg, sizeof(msg));
				return;
			}

			/* 发送结束标志 */
			free(pDir);
			msg.info.file_info.type = 0;
			evbuffer_add(output, &msg, sizeof(msg));
			break;
		case COMMAND_FILE_DOWNLOAD: 		/* 下载群文件 */
			/* 子线程传输文件传参初始化 */
			p = (TranFileThread *)malloc(sizeof(TranFileThread));
			p->msg = msg;
			p->bev = bev;
			tm_node = (ThreadMutexNode *)malloc(sizeof(ThreadMutexNode));
			tm_node->bev = bev;
			Insert_LinkList_Pos(tm_list, 1, (LinkNode *)tm_node);

			/* 创建子线程 */
			pthread_create(&p->tid, &attr, file_download, (void *)p);

			/* 线程分离 */
			pthread_detach(p->tid);
			break;
		case COMMAND_EXIT_CHATGROUP: 		/* 离开了群聊 */
			/* 提示所有在线群成员，该用户离开了 */
			gum_node = (GroupUserMapNode *)malloc(sizeof(GroupUserMapNode));
			for(LinkNode *p = user_list->head.next; p != &user_list->tail;
					p = p->next)
			{
				/* 判断是否在线 */
				if(((UserNode *)p)->status != USER_STAT_ONLINE)
					continue;

				/* 找到该群成员 */
				gum_node->uid = ((UserNode *)p)->user.id;
				gum_node->gid = msg.group.id;
				ret = Find_LinkList(group_user_map_list, (LinkNode *)gum_node,
						Compare_GroupUserMapNode_Uid_Gid);
				if(ret == -1)
					continue;

				/* 发送通知 */
				bufferevent_write(((UserNode *)p)->bev, &msg, sizeof(msg));
			}
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
void write_cb(struct bufferevent *bev, void *arg)
{
//	printf("成功写数据给客户端,写缓冲区回调函数被回调.\n");
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  event_cb
 *  Description:  
 * =====================================================================================
 */
void event_cb(struct bufferevent *bev,short events, void *arg)
{
	if (events & BEV_EVENT_EOF)
	{
		printf("连接关闭。\n");
	} 
	else if(events & BEV_EVENT_ERROR)
	{
		printf("连接出现错误！\n");
	}

	bufferevent_free(bev);

	printf("bufferevent 资源已经被释放.\n");
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  signal_cb
 *  Description:  
 * =====================================================================================
 */
void signal_cb(evutil_socket_t sig, short events, void *arg)
{
	struct event_base *base = (struct event_base *)arg;
	struct timeval delay = {2, 0};

	write_data();

	printf("\n接受到关闭信号，程序将在2秒后退出。。。\n");

	event_base_loopexit(base, &delay);
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
	int cnt = tft->msg.info.file_info.size / FILE_ONCE_SIZE + 1;
	FILE *fp;
	char path[PATH_MAX];
	ThreadMutexNode *tm_node;

	/* 在本地创建该文件 */
	snprintf(path, PATH_MAX, "group_file/group_%d/%s", tft->msg.group.id, tft->msg.info.file_info.name);
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

	tm_node = (ThreadMutexNode *)malloc(sizeof(ThreadMutexNode));
	tm_node->bev = tft->bev;
	Remove_LinkList(tm_list, (LinkNode *)tm_node, Compare_ThreadMutexNode);
	free(tm_node);
	free(tft);
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
	FILE *fp;
	char path[PATH_MAX];
	struct stat info;
	struct evbuffer *output = bufferevent_get_output(tft->bev);

	/* 将服务器端文件数据传回 */
	sprintf(path, "group_file/group_%d/%s", tft->msg.group.id, tft->msg.info.file_info.name);
	lstat(path, &info);
	tft->msg.info.file_info.size = info.st_size;
	cnt = info.st_size / FILE_ONCE_SIZE + 1;
	evbuffer_add(output, &tft->msg, sizeof(tft->msg));

	/* 从服务器读取该文件 */
	fp = fopen(path, "r");
	if(!fp)
		perr_exit("fopen error");

	/* 分cnt次传输 */
	tft->msg.info.file_data.type = FILE_DOWNLOAD_ING;
	for(int i = 0; i < cnt; i++)
	{
		tft->msg.info.file_data.offset = i * FILE_ONCE_SIZE;
		if(i + 1 == cnt) 		/* 最后一次，传输剩余的大小 */
			tft->msg.info.file_data.len = info.st_size % FILE_ONCE_SIZE;
		else 					/* 每次，固定传输大小 */
			tft->msg.info.file_data.len = FILE_ONCE_SIZE;

		fread(tft->msg.info.file_data.buf, sizeof(char), tft->msg.info.file_data.len, fp);
		evbuffer_add(output, &tft->msg, sizeof(tft->msg));
	}
	fclose(fp);

	/* 发送结束标志 */
	tft->msg.info.file_data.type = FILE_DOWNLOAD_END;
	evbuffer_add(output, &tft->msg, sizeof(tft->msg));

	free(tft);
	return NULL;
}



