/*************************************************************************
	> File Name: mv.c
	> Created Time: Sun 26 Apr 2020 10:44:01 AM CST
 ************************************************************************/

#include <apue.h>

void myMv(const char *filename1, const char*filename2);

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("To few arguments!\n");
		exit(-1);
	}

	myMv(argv[1], argv[2]);

    return 0;
}

void myMv(const char *filename1, const char*filename2)
{
/*	link(filename1, filename2);
	unlink(filename1);*/
	rename(filename1, filename2);
}

