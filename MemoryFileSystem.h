//
// Created by zfy on 2019/6/19.
//
#ifndef MEMORYFILESYSTEM_MEMORYFILESYSTEM_H
#define MEMORYFILESYSTEM_MEMORYFILESYSTEM_H
#endif //MEMORYFILESYSTEM_MEMORYFILESYSTEM_H

/***********************/
//磁盘管理方式 = 位示图 + 连续分配（单位Byte）
#define VirtualDiskSize 8*1024*1024   //虚拟磁盘存储空间8M
#define BlockSize 1024                //盘块大小，1KB
#define ContentSize 1000              //一个盘块能写的内容长度（字节数）
#define BlockCount VirtualDiskSize/BlockSize  //虚拟磁盘的盘块数 8k个
#define FCB_size  64     //文件控制块大小，64B，一个磁盘存储16个，分配128个盘块，0-7盘块用于存储位示图，8-127用来存储FCB
#define BitMapCount 8    //位示图占用的盘块数
#define FCB_count 1920   //FCB总个数，1920 = 16 * 120
#define FileBlockCount 8064   //用户空间的盘块数 8*1024 - 128 = 8064
#define SystemBlockCount 128  //系统空间盘块数


struct BLOCK {
    char content[ContentSize];
    int fileLength;    //文件内容长度
    int blockID;       //盘块标识
    struct BLOCK *nextBlock;;
}BLOCK={'\0',0,-1,NULL};

struct FCB{
    char name[28];
    int type;
    int size;
    struct BLOCK *block;
    struct FCB *parent;
    struct FCB *brother;
    struct FCB *child;
}FCB = {"",1,0,NULL,NULL,NULL,NULL};

struct StringList{
    char *string;
    struct StringList *next;
};

struct User{
    struct FCB *EnterFileSystem;   //用户进入文件系统的入口；
};

extern char *VirtualDiskAddr;
extern struct FCB *emptyFCB;
extern struct FCB *lastFCB;
extern struct BLOCK *emptyBLOCK;
extern struct BLOCK *lastBLOCK;

extern int emptyFCB_count;
extern int emptyBLOCK_count;

extern struct FCB *current;
extern char *current_path;

extern struct FCB *root;

void                                                                                                                                                                                                           ;
struct FCB* returnFCB(char *path_name,char* name,int type,struct FCB *parent);
struct StringList* dir(char *dirpath);
int mkdir(char *dirpath);
int rmdir(char *dirpath);
int CreateFile(char *filepath);
int DeleteFile(char *filepath);
int ReName(char *path,int type,char *new_name);
int WriteFile(struct FCB *file,char *content);
char* ReadFile(struct FCB *file);
int cd(char *path,char *name);
void cdback();
int cut(char *oldPath, int type, char *newPath);//剪切；
int copy(char *oldPath, int type, char *newPath);//复制；










