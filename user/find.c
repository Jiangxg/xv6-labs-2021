// find.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target) {
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

  // 递归终止条件
	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		printf("find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch(st.type){
	case T_FILE:
		// 如果文件名结尾匹配 `/target`，则视为匹配
		if(strcmp(path+strlen(path)-strlen(target), target) == 0) {
			printf("%s\n", path);
		}
		break;
	case T_DIR:
    // DIRSIZ表示xv6支持的最大的文件名长度14，定义在kernel/fs.h中
		if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("find: path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf+strlen(buf);
		*p++ = '/';

    // de是struct:
    // struct dirent {
    //    ushort inum;
    //    char name[DIRSIZ];
    //  };
    // 

    // 一个一个的读取文件夹下的文件名
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			if(de.inum == 0)
				continue;

			memmove(p, de.name, DIRSIZ);
      
      // 0 即 NULL 是字符串的结尾符
			p[DIRSIZ] = 0;

			if(stat(buf, &st) < 0){  //fstate 接受文件描述符作为参数，stat接受文件路径作为参数；正常打开返回0，发生错误返回-1
				printf("find: cannot stat %s\n", buf);
				continue;
			}
			// 不要进入 `.` 和 `..`
			if(strcmp(buf+strlen(buf)-2, "/.") != 0 && strcmp(buf+strlen(buf)-3, "/..") != 0) {
				find(buf, target); // 递归查找
			}
		}
		break;
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	if(argc < 3){
		exit(0);
	}
	char target[512];
	target[0] = '/'; // 为查找的文件名添加 / 在开头
	strcpy(target+1, argv[2]);
	find(argv[1], target);
	exit(0);
}