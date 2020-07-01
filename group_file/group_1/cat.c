/*************************************************************************
	> File Name: cat.c
	> Created Time: Sun 26 Apr 2020 10:37:29 AM CST
 ************************************************************************/

#include <apue.h>

#define LINEMAX 4096

void myCat(const char *filename);

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("To few arguments!\n");
		exit(-1);
	}

	for(int i = 1; i < argc; i++)
		myCat(argv[i]);

    return 0;
}

void myCat(const char *filename)
{
	FILE *fp;
	char buf[LINEMAX];
	int len;

	fp = fopen(filename, "r");
	if(!fp)
		err_ret("cat: %s", filename);

	while((len = fread(buf, sizeof(char), LINEMAX-1, fp)) > 0)
		fwrite(buf, sizeof(char), len, stdout);

	fclose(fp);
}

