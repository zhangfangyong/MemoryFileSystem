#include <stdio.h>
#include "MemoryFileSystem.h"

#define DirDepth 20            //子目录个数

int numFCB, numBLOCK;         //全局变量，复制是计算需要的资源；

char *VirtualDiskAddr;
struct FCB *emptyFCB;
struct FCB *lastFCB;
struct BLOCK *emptyBLOCK;
struct BLOCK *lastBLOCK;

int emptyFCB_count;
int emptyBLOCK_count;

struct FCB *current;
char *current_path;

struct FCB *root;


void InitDisk(){
    VirtualDiskAddr = (char *)malloc(VirtualDiskSize * sizeof(char));

    //初始化位示图
    for (int i = 0; i < BlockCount; i++) {
        VirtualDiskAddr[i] = '0';
    }

    for (int j = 0; j < SystemBlockCount ; j++) {
        VirtualDiskAddr[j] = '1';
    }

    //根root
    root = (struct FCB *) new(VirtualDiskAddr + BitMapCount * BlockSize);
    strcpy(root->name,"root");
    root->parent = root;
    current = root;
    current_path = "//";

    //建立空FCB链表
    emptyFCB = (struct FCB *) new(root + 1);
    lastFCB = emptyFCB;
    for (int k = 0; k < FCB_count-1 ; k++) {
        lastFCB->child = (struct FCB*) new(lastFCB + 1);
        lastFCB = lastFCB->child;
    }
    emptyFCB_count = FCB_count - 1;

    //建立空BLOCK链表
    emptyBLOCK = (struct BLOCK *) new(VirtualDiskAddr + SystemBlockCount * BlockSize);
    emptyBLOCK->blockID = SystemBlockCount;
    lastBLOCK = emptyBLOCK;
    for (int l = 0; l < BlockCount ; l++) {
        lastBLOCK->nextBlock = (struct BLOCK *) new(lastBLOCK + 1);
        lastBLOCK = lastBLOCK->nextBlock;
        lastBLOCK->blockID = SystemBlockCount + l;
    }
    emptyBLOCK_count = BlockCount;
}
struct FCB* GetFCB(){
    if(emptyFCB_count > 0){           //此处不同
        struct FCB* fcb = emptyFCB;
        emptyFCB = emptyFCB->child;
        fcb->child = NULL;
        emptyFCB_count--;
        return fcb;
    }
    return NULL;
}

struct BLOCK* GetBlock(int needBlockNum){
    if(emptyBLOCK_count > needBlockNum){         //此处不同
        struct BLOCK* block = emptyBLOCK;
        for (int i = 0; i < needBlockNum - 1 ; i++) {
            emptyBLOCK = emptyBLOCK->nextBlock;
        }
        struct BLOCK* temp = emptyBLOCK;
        emptyBLOCK = emptyBLOCK->nextBlock;
        temp->nextBlock = NULL;
        emptyBLOCK_count -= needBlockNum;
        return block;
    }
    return NULL;
}

void RecoverBLOCK(struct BLOCK* block){
    if(block == NULL) return;
    lastBLOCK->nextBlock = block;
    lastBLOCK = lastBLOCK->nextBlock;
    strcpy(lastBLOCK->content,"");
    while (lastBLOCK != NULL){
        lastBLOCK->fileLength = 0;
        emptyBLOCK_count++;
        lastBLOCK = lastBLOCK->nextBlock;
    }
}

void RecoverFCB(struct FCB* fcb){
    lastFCB->child = fcb;
    lastFCB = lastFCB->child;
    RecoverBLOCK(lastFCB->block);
    strcpy(lastFCB->name,"");
    lastFCB->type = 1;
    lastFCB->size = 0;

    lastFCB->block = NULL;
    lastFCB->child = NULL;
    lastFCB->parent = NULL;
    lastFCB->brother = NULL;

    emptyBLOCK_count++;
}

struct FCB* ReturnSonFCB(struct FCB* currentDir, char* name,int type){
    struct FCB* temp = currentDir;
    while (temp != NULL){
        if(temp->name == name && temp->type == type){
            return temp;
        }
        temp = temp->brother;
    }
    return NULL;
}

struct FCB* ReturnDirFCB(char* Dir[],int count, _Bool isAbsolutePath){
    struct FCB* temp;
    if(isAbsolutePath){
        temp = root;
    } else {
        temp = current;
    }
    for (int i = 0; i < count ; i++) {
        temp = ReturnSonFCB(temp,Dir[i],1);
        if (temp == NULL){
            return NULL;
        }
    }
    return temp;
}

struct FCB* returnFCB(char* path_name,char* name,int type,struct FCB *parent){
    char* DIR[DirDepth];
    int i = 0;
    for (int j = 0; j < sizeof(path_name) ; ++j) {
        if(path_name[j] == '/'){
            if(DIR[i] != ""){
                i++;
            } else continue;
        } else {
            DIR[i] += path_name[j];
        }
    }
    int count = 0;
    for (int k = 0; k < 20 ; k++) {
        if(DIR[k] != ""){
            count++;
        } else {
            break;
        }
    }
    if(count == 0 && path_name[0] == '/'){
        name = "root";
        parent = root;
        return root;
    } else {
        name = DIR[count - 1];
    }
    _Bool AbsolutePath = 0;
    if(path_name[0] == '/'){
        AbsolutePath = 1;
    }
    parent = ReturnDirFCB(DIR,count - 1,AbsolutePath);
    if(parent == NULL){
        return ReturnSonFCB(parent,name,type);
    } else {
        return NULL;
    }
}

void Delete_Dir_or_File(struct FCB* deleting){
    struct FCB* temp = deleting->parent;
    if(temp->child == deleting){
        temp->child = deleting->brother;
    } else {
        temp = temp->child;
        while (temp->brother != deleting){
            temp = temp->brother;
        }
        temp->brother = deleting->brother;
    }

    if(deleting->type == 1){
        while (deleting->child != NULL){
            Delete_Dir_or_File(deleting->child);
        }
        RecoverFCB(deleting);
    } else {
        RecoverFCB(deleting);
    }
}

void num_Dir_or_File(struct FCB* add){
    numFCB++;
    struct FCB* temp;
    temp = add->child;
    if(temp == NULL){
        double NeedAddBlock;
        NeedAddBlock = add->size / ContentSize;
        int tmp = NeedAddBlock;
        if(NeedAddBlock == tmp){
            numBLOCK += tmp;
        } else {
            numBLOCK += tmp + 1;
        }
    } else {
        while (temp->brother != NULL){
            num_Dir_or_File(temp->brother);
            temp = temp->brother;
        }
        num_Dir_or_File(add->child);
    }
}

void add_Dir_or_File(struct FCB* adding,struct FCB* destination){

    //destination ，复制到的那个目录；adding，需要复制的内容；
    //因为要增加这个文件/目录，所以都需要把这个节点复制一份移入目录树中；
    struct FCB* copy_adding = GetFCB();
    strcpy(copy_adding->name,adding->name);

    copy_adding->type = adding->type;
    copy_adding->size = adding->size;
    copy_adding->block = adding->block;

    //下面两行实现了移入目录树中；
    copy_adding->parent = destination;
    copy_adding->brother = destination->child;
    destination->child = copy_adding;

    //这是区分目录和文件的关键；
    copy_adding->child = NULL;

    struct FCB* temp = adding->child;

    if(temp == NULL){
        //说明adding是个文件；
        //则需要add声请BLOCK，存放内容；

        //该文件有内容，需要增添BLOCK；

        //需要添加几个；
        double needAddBlock;
        needAddBlock = copy_adding->size / ContentSize;
        if(needAddBlock == 0){
            //不需要Block
        } else {
            int tmp = needAddBlock;
            struct BLOCK* contentFirstAdrr;   //分配的BLOCK的首地址；
            if(needAddBlock == tmp){
                //说明没损失，所以分配的个数为tmp;
                contentFirstAdrr = getBLOCK(tmp);
            } else {
                //精度损失；上取整；
                contentFirstAdrr = getBLOCK(tmp + 1);
            }

            copy_adding->block = contentFirstAdrr;

            //指向adding的BLOCK；
            struct BLOCK* tmp_addingBLOCK;
            tmp_addingBLOCK = adding->block;

            for (int i = 0; i < tmp - 1 ; i++) {

            }
        }
    }
}