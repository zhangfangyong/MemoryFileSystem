//
// Created by zfy on 2019/6/22.
//

#pragma once


#include <string>
using namespace std;

/**************************************************/
//磁盘管理：位示图+连续分配(默认单位为Byte)
#define _virtualDiskSize 8*1024*1024    //定义虚拟磁盘(txt文件充当)的大小为8M；  写入的都是字母等，所以比较小；
#define _blockSize 1024          //盘块大小为1KB；
#define _contentSize 1000          //一个BLOCK能写的字节数；
#define _blockCount _virtualDiskSize/_blockSize //虚拟磁盘的盘块个数             //8k个；  如果用字符0/1标识是否占用盘块，这种位示图表示方式，需要8个；如果用二进制位0/1，表示的话，一个字符是八个二进制位，只需要1个磁盘块(目前不知道实现方式)；
#define _FCB_size 64                //FCB定长管理，64B；             //则一个磁盘块可以存储16个； 分配128个磁盘块存储目录树； 0-7磁盘块位式图；8-127；(认为0-127，是系统空间)剩余的磁盘块都用来存储文件内容，通过指针指向；128-8191(用户空间)；
#define _bitMapCount 8                  //位式图占用的磁盘块个数
#define _FCB_count 1920                  //FCB总个数；
#define _BLOCK_count 8064                  //BLOCK总个数；
#define _systemUsedBlock 128               //系统使用的block总数

struct BLOCK {
    char _content[1000];
    char _spare[11];                //这个跳过；多余的；
    int _blockLength;                  //文件内容长度；
    int _blockID;                      //这个block的ID；
    BLOCK* _nextBlock;
    BLOCK() {
        for (int i = 0; i < 11; i++)
            _spare[i] = '\0';

        this->_blockLength = 0;
        this->_blockID = -1;
        this->_nextBlock = NULL;
    }
};


/*******************************************************/

//目录管理：基于FCB结点的树形结构，采用孩子兄弟表示法；

//一个是64B；
struct FCB {
    char _name[28];              //默认28B；
    char _spare[12];
    int _type;                 //int 是4B；
    int _size;
    BLOCK* _block;             //指针默认是4B
    FCB* _parent;
    FCB* _brother;
    FCB* _child;
    FCB() {
        for (int i = 0; i < 12; i++)
            _spare[i] = '\0';
        strcpy_s(this->_name, "");
        this->_type = 1;                      //默认为目录，1为目录；
        this->_size = 0;

        this->_block = NULL;
        this->_parent = NULL;
        this->_brother = NULL;
        this->_child = NULL;
    }
};
struct StringList
{
    string content;
    StringList*	next;

    /*StringList(string inputString) {
    this->content=inputString ;
    next = NULL;
    }*/
};

struct User {
    //list<FCB> User_List;	//多用户；但本系统只实现单用户
    FCB* _EnterFileSystem;   //用户进入文件系统的入口；
};



extern char* _virtualDiskAddr;   //虚拟磁盘的入口地址；

extern FCB* _emptyFCB;    //空FCB列表；
extern FCB* _lastFCB;    //空FCB列表的末尾；为了回收设计的；

extern BLOCK* _emptyBLOCK;    //空BLOCK列表；
extern BLOCK* _lastBLOCK;    //空BLOCK列表的末尾；为了回收设计的；

extern int _emptyBLOCK_Count;      //可用的BLOCK数量；
extern int _emptyFCB_Count;         //可用的FCB的数量；

extern FCB* _current; //当前目录；
extern string current_path; //当前路径；

extern FCB* _root;   //指向root，实现绝对路径；

/**********************************************************************/
//磁盘管理声明；
/**********************************************************************/
//目录管理声明；
/**********************************************************************/
void sys_initDisk(); //初始化虚拟磁盘；                                      //恢复磁盘测试后才知道是否正确；
FCB* sys_returnFCB(string path_name, string &name, int type, FCB* &parent);  //目前完善成功；
StringList* sys_dir(string dirPath);//查看当前目录内容；				//目前测试成功；
int sys_mkdir(string dirPath);                                         //目前测试成功；
int sys_rmdir(string dirPath);//删除目录;							 //目前测试成功；
int sys_create_file(string filePath);//创建文件；                     //目前测试成功；
int sys_delete_file(string filePath);//删除文件；                       //目前测试成功；
int sys_rename(string path, int type, string new_name);//重命名文件；    //目前测试成功；
int sys_write_file(FCB* file, string content);//向文件写内容；   //目前测试成功；
string sys_read_file(FCB* file);//读取文件内容；                //目前测试成功；
int sys_cd(string path, string &name);//进入子目录；			//目前测试成功；
void sys_cdback();//返回上一级目录；								//目前测试成功；
int sys_cut(string oldPath, int type, string newPath);//剪切；			//目前测试成功；
int sys_copy(string oldPath, int type, string newPath);//复制；         //目前测试，成功；


