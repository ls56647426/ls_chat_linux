/*
 * =====================================================================================
 *
 *       Filename:  menu.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2020年05月26日 22时00分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  longshao (ls), ls56647426@foxmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _MENU_H_
#define _MENU_H_

/* c std lib */
#include <stdio.h>

/* third-party lib */
#include <ls/io.h>

void showInitMenu();

void showLoginMenu();

void showRegisterMenu();

void showHomeMenu();

void showCreateGroupMenu();

void showAddUserMenu();

void showAddGroupMenu();

void showSelectChatUserMenu();

void showSelectChatGroupMenu();
	
void showChatGroupMenu();

void showSelectFileUpdate();

void showChatUserMenu();

void showFileUploadMenu();

void showFileDownloadMenu();

#endif

