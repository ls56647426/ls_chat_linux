/*
 * =====================================================================================
 *
 *       Filename:  packet.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2020年05月24日 09时11分04秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  longshao (ls), ls56647426@foxmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "packet.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Get_UserNode_Info
 *  Description:  
 * =====================================================================================
 */
void Get_UserNode_Name(LinkNode *node, char **text, Color *color)
{
	UserNode *user = (UserNode *)node;

	strpcpy(text, user->user.name);
	strpcpy(&color->color, GREEN);
	strpcpy(&color->bgcolor, NONE);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Print_UserNode
 *  Description:  
 * =====================================================================================
 */
void Print_UserNode(const LinkNode *node)
{
	UserNode *user = (UserNode *)node;

	printf("User:{id = %d, name = %s, status = %d}\n", user->user.id, user->user.name,
			user->status);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_UserNode
 *  Description:  
 * =====================================================================================
 */
int Compare_UserNode(const LinkNode *node1, const LinkNode *node2)
{
	UserNode *user1 = (UserNode *)node1;
	UserNode *user2 = (UserNode *)node2;

	return strcmp(user1->user.name, user2->user.name);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_UserNode_Id
 *  Description:  
 * =====================================================================================
 */
int Compare_UserNode_Id(const LinkNode *node1, const LinkNode *node2)
{
	UserNode *user1 = (UserNode *)node1;
	UserNode *user2 = (UserNode *)node2;
	
	return user1->user.id - user2->user.id;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_UserUserMapNode
 *  Description:  
 * =====================================================================================
 */
int Compare_UserUserMapNode(const LinkNode *node1, const LinkNode *node2)
{
	UserUserMapNode *map1 = (UserUserMapNode *)node1;
	UserUserMapNode *map2 = (UserUserMapNode *)node2;

	return map1->id - map2->id;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_UserUserMapNode_Uid
 *  Description:  
 * =====================================================================================
 */
int Compare_UserUserMapNode_Uid(const LinkNode *node1, const LinkNode *node2)
{
	UserUserMapNode *map1 = (UserUserMapNode *)node1;
	UserUserMapNode *map2 = (UserUserMapNode *)node2;

	if(map1->uid1 == map2->uid1)
		return map1->uid2 - map2->uid2;
	
	if(map1->uid1 == map2->uid2)
		return map1->uid2 - map2->uid1;
	
	if(map1->uid2 == map2->uid1)
		return map1->uid1 - map2->uid2;

	if(map1->uid2 == map2->uid2)
		return map1->uid1 - map2->uid1;

	return map1->uid1 - map2->uid1;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Print_GroupNode
 *  Description:  
 * =====================================================================================
 */
void Print_GroupNode(const LinkNode *node)
{
	GroupNode *group = (GroupNode *)node;

	printf("Group:{id = %d, name = %s}\n", group->group.id, group->group.name);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_GroupNode
 *  Description:  
 * =====================================================================================
 */
int Compare_GroupNode(const LinkNode *node1, const LinkNode *node2)
{
	GroupNode *group1 = (GroupNode *)node1;
	GroupNode *group2 = (GroupNode *)node2;

	return strcmp(group1->group.name, group2->group.name);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Print_GroupUserMapNode
 *  Description:  
 * =====================================================================================
 */
void Print_GroupUserMapNode(const LinkNode *node)
{
	GroupUserMapNode *map = (GroupUserMapNode *)node;

	printf("GroupUserMap:{id = %d, uid = %d, gid = %d, status = %d, permission = %d}\n",
			map->id, map->uid, map->gid, map->status, map->permission);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_GroupUserMapNode
 *  Description:  
 * =====================================================================================
 */
int Compare_GroupUserMapNode(const LinkNode *node1, const LinkNode *node2)
{
	GroupUserMapNode *map1 = (GroupUserMapNode *)node1;
	GroupUserMapNode *map2 = (GroupUserMapNode *)node2;

	return map1->id - map2->id;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_GroupUserMapNode_Uid_Gid
 *  Description:  
 * =====================================================================================
 */
int Compare_GroupUserMapNode_Uid_Gid(const LinkNode *node1, const LinkNode *node2)
{
	GroupUserMapNode *map1 = (GroupUserMapNode *)node1;
	GroupUserMapNode *map2 = (GroupUserMapNode *)node2;

	if(map1->gid == map2->gid)
		return map1->uid - map2->uid;
	return map1->gid - map2->gid;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare_ThreadMutexNode
 *  Description:  
 * =====================================================================================
 */
int Compare_ThreadMutexNode(const LinkNode *node1, const LinkNode *node2)
{
	ThreadMutexNode *tm1 = (ThreadMutexNode *)node1;
	ThreadMutexNode *tm2 = (ThreadMutexNode *)node2;

	return (long long)tm1->bev - (long long)tm2->bev;
}




