/*************************************************************************
	> File Name: packet.h
	> Created Time: 2020年05月23日 星期六 09时42分34秒
	> Author: ls
 *************************************************************************/

#ifndef _PACKET_H_
#define _PACKET_H_

/* c std lib */
#include <stdio.h>
#include <string.h>

/* unix lib */
#include <netinet/in.h>

/* third-party lib */
#include <ls/LinkTable.h>

/* ++++++++++++++++++++++++++++++ 用户 ++++++++++++++++++++++++++++++ */

/**
 * 用户结构体：User
 * id：用户唯一标识id
 * 		id不为0，如果为0，代表不管这个用户数据
 * name：用户名
 */
#define USER_NAME_LEN 16
typedef struct
{
	int id;
	char name[USER_NAME_LEN];
} User;

/**
 * 用户结点：UserNode
 * node：结点
 * user：用户数据
 * status：当前状态
 * 		USER_STAT_ONLINE：在线
 * 		USER_STAT_OFFLINE：离线
 * 		USER_STAT_HIDE：隐身
 * bev：当前会话的bufferevent
 * login_up：当前登录地点的ip
 * tid：开子线程时候的线程id
 *
 * 封装函数：
 * 1. Print_UserNode：打印
 * 		打印node内容
 * 2. Compare_UserNode：比较
 * 		node1和node2比较
 */
#define USER_STAT_ONLINE 0
#define USER_STAT_OFFLINE 1
typedef struct
{
	LinkNode node;
	User user;
	int status;
	struct bufferevent *bev;
	char login_ip[INET_ADDRSTRLEN];
} UserNode;
void Get_UserNode_Name(LinkNode *node, char **text, Color *color);
void Print_UserNode(const LinkNode *node);
int Compare_UserNode(const LinkNode *node1, const LinkNode *node2);
int Compare_UserNode_Id(const LinkNode *node1, const LinkNode *node2);

/* ------------------------------ 用户 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 用户 - 用户 映射表 ++++++++++++++++++++++++++++++ */

/**
 * 用户 - 用户 映射表
 *  	用户1 和 用户2 是好友
 * node：结点
 * id：唯一标识id
 * uid1：用户1的id
 * uid2：用户2的id
 */
typedef struct
{
	LinkNode node;
	int id;
	int uid1;
	int uid2;
} UserUserMapNode;
int Compare_UserUserMapNode(const LinkNode *node1, const LinkNode *node2);
int Compare_UserUserMapNode_Uid(const LinkNode *node1, const LinkNode *node2);

/* ------------------------------ 用户 - 用户 映射表 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 群 ++++++++++++++++++++++++++++++ */

/**
 * 群结构体：Group
 * id：群唯一标识id
 * 		id不为0，如果为0，代表不在群里
 * name：群名称
 */
#define GROUP_NAME_LEN 32
typedef struct
{
	int id;
	char name[GROUP_NAME_LEN];
} Group;

/**
 * 群内的用户数据结点：GroupUserNode
 * user_node：用户结点
 * 
 * 群结点：GroupNode
 * node：结点
 * group：群数据
 *
 * 函数封装：
 * 1. Print_GroupNode		打印
 * 		打印node内容
 * 2. Compare_GroupNode		比较
 * 		比较node1和node2
 */
typedef struct
{
	LinkNode node;
	Group group;
} GroupNode;
void Print_GroupNode(const LinkNode *node);
int Compare_GroupNode(const LinkNode *node1, const LinkNode *node2);

/* ------------------------------ 群 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 群 - 用户 映射表 ++++++++++++++++++++++++++++++ */

/**
 * 群 - 用户 关联映射表
 * 		用户uid在群gid中
 * node：结点
 * id：映射表唯一标识id
 * uid：用户id
 * gid：群id
 * status：群内用户状态
 * 		GROUP_USER_STAT_NORMAL：正常
 * 		GROUP_USER_STAT_BANPOST：禁言
 * permission：权限
 * 		PREM_USER：普通用户
 * 		PREM_ADMI：管理员
 *
 * 函数封装：
 * 1. Print_GroupUserMapNode		打印
 * 		打印node内容
 * 2. Compare_GroupUserMapNode		比较
 * 		比较node1和node2
 */
#define GROUP_USER_STAT_NORMAL 0
#define GROUP_USER_STAT_BANPOST 1
#define PERM_USER 0
#define PERM_ADMI 1
typedef struct
{
	LinkNode node;
	int id;
	int uid;
	int gid;
	int status;
	int permission;
} GroupUserMapNode;
void Print_GroupUserMapNode(const LinkNode *node);
int Compare_GroupUserMapNode(const LinkNode *node1, const LinkNode *node2);
int Compare_GroupUserMapNode_Uid_Gid(const LinkNode *node1, const LinkNode *node2);

/* ++++++++++++++++++++++++++++++ 文件 ++++++++++++++++++++++++++++++ */

/* ------------------------------ 文件 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 传输 ++++++++++++++++++++++++++++++ */

/**
 * 文件数据传输
 *		需要先传一个文件名和大小，再分成多个以下结构体传输该文件
 *		后面需要分几次完成传输
 *
 * type：数据类型
 * filename：文件名
 * size：文件大小
 *
 * type：数据类型
 * offset：偏移
 * len：长度
 * buf：数据
 */
#define FILE_UPLOAD_HEAD 		0 		/* 文件上传头 */
#define FILE_UPLOAD_ING 		1 		/* 文件上传中 */
#define FILE_UPLOAD_END 		2 		/* 文件上传结束 */
#define FILE_DOWNLOAD_HEAD 		3 		/* 文件下载头 */
#define FILE_DOWNLOAD_ING 		4 		/* 文件下载中 */
#define FILE_DOWNLOAD_END 		5 		/* 文件下载结束 */
#define FILE_SELECT_HEAD 		6 		/* 文件查看头 */
#define FILE_SELECT_ING 		7 		/* 文件查看中 */
#define FILE_SELECT_END 		8 		/* 文件查看结束 */

#define FILE_NAME_LEN 128
#define FILE_ONCE_SIZE 400
typedef struct
{
	int type;
	char name[FILE_NAME_LEN];
	int size;
} FileInfo;

typedef struct
{
	int type;
	int offset;
	int len;
	char buf[FILE_ONCE_SIZE];
} TranFile;

/* ------------------------------ 传输 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 消息 ++++++++++++++++++++++++++++++ */

/**
 * 消息结构体：Msg
 * type：消息类型
 * 		MSG_REGISTER：注册
 * 		MSG_SENDMSG：发送消息
 * 		MSG_OFFLINE：下线
 * group：群信息
 * 		group.id为0时，代表私聊。非0时，代表群聊
 * src：谁传的消息
 * dest：这个消息要传给谁
 * 		群里时，dest.id为0，代表传给群里所有用户。非0时，代表通过群进行的私聊
 * info：消息内容
 * 		info：发送的消息内容
 * 		file_info：文件消息
 */
#define MSG_INFO_LEN 256

#define MSG_REGISTER 					0 		/* 注册 */
#define MSG_LOGIN 						1 		/* 登录 */
#define MSG_SENDMSG 					2 		/* 发送消息 */
#define MSG_LOGOUT 						3 		/* 退出 */
#define MSG_TRANFILE 					4 		/* 发送文件 */
/* 给消息预留出 0~255 */

#define COMMAND_GET_ONLINEUSER 			256 	/* 获取好友在线用户列表 */
#define COMMAND_GROUP_BANUSERPOST 		257 	/* 群内禁言 */
#define COMMAND_GROUP_DELETEUSER 		258 	/* 踢人 */
#define COMMAND_FILE_UPLOAD 			259		/* 上传文件 */
#define COMMAND_FILE_DOWNLOAD 			260 	/* 下载文件 */
#define COMMAND_CREATE_GROUP 			261 	/* 创建群 */
#define COMMAND_APPLY_ADD_GROUP 		262 	/* 申请加群 */
#define COMMAND_ADMINIST_ADD_GROUP 		263 	/* 管理加群申请 */
#define COMMAND_GET_GROUP_FILE 			264 	/* 获取群文件 */
#define COMMAND_ADD_USER 				265 	/* 加好友 */
#define COMMAND_GET_GROUP_ONLINEUSER 	266		/* 获取群成员在线列表 */
#define COMMAND_INTO_GROUP 				267 	/* 进群聊天 */
#define COMMAND_INTO_CHATUSER 			268 	/* 进入私聊 */
#define COMMAND_FILE_UPLOAD_ING 		269 	/* 正在上传群文件 */
#define COMMAND_EXIT_CHATGROUP 			270 	/* 离开了群聊 */
/* 给命令预留出 256~511 */

#define ERRNO_SUCCESS 					512 	/* 成功 */
#define ERRNO_ALREADYEXIST 				513 	/* 该用户已存在 */
#define ERRNO_INEXISTENCE 				514 	/* 该用户不存在 */
#define ERRNO_NOTFOUND 					515 	/* 未找到该用户 */
#define ERRNO_BEOFFLINE 				516 	/* 被挤掉了，下线 */
#define ERRNO_GROUP_ALREADYEXIST 		517 	/* 该群已存在 */
#define ERRNO_GROUP_INEXISTENCE 		518 	/* 该群不存在 */
#define ERRNO_ADDUSER_NOTFOUND 			519 	/* 加好友，没找到该用户 */

typedef union
{
	char info[MSG_INFO_LEN];
	FileInfo file_info;
	TranFile file_data;
} MsgInfo;

typedef struct
{
	int type;
	Group group;
	User src;
	User dest;
	MsgInfo info;
} Msg;

/* ------------------------------ 消息 ------------------------------ */

/* ++++++++++++++++++++++++++++++ 子线程封装 ++++++++++++++++++++++++++++++ */

/**
 * 子线程传输文件封装
 *
 * msg：客户端传来的msg
 * tid：子线程id
 * bev：event的缓冲区
 */
typedef struct
{
	Msg msg;
	pthread_t tid;
	struct bufferevent *bev;
} TranFileThread;

/**
 * 子线程读缓冲区互斥 结点
 */
typedef struct
{
	LinkNode node;
	struct bufferevent *bev;
} ThreadMutexNode;
int Compare_ThreadMutexNode(const LinkNode *node1, const LinkNode *node2);

/* ------------------------------ 子线程封装 ------------------------------ */


#endif

