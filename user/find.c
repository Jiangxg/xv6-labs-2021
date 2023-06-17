// find.c
// modified from ls
// 在目录树中找到符合特定文件名的所有文件
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h" //需要涉及到文件系统

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}


void
find(char *path, char *target){
  char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;
  
  
  // first, varify the path 
  if ((fd = open(path, 0 )) < 0) {
    printf("%s does not exist\n", path);
    return;
  }
  
  if (fstat(fd, &st) < 0) {
    printf("can not stat %s\n", path);
    close(fd); // close file before return
    return;
  }

  switch(st.type){
    case T_FILE:
      pass;
      break;
    case T_DIR:
      pass;
      break;
  }

  close(fd)

}



int
main(int argc, char *argv[])
{
  if(argc < 3){ // need 3 arguments
    exit(1);
  }

  char target[512]; //why is 512?
  target[0] = '/'; // the target begin with '/' is convenient
  
  find(argv[1], target);
  exit(0)
}
