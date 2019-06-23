//
// Created by zfy on 2019/6/22.
//

#include"stdafx.h"
#include "SystemFunction.h"
#include "MemoryFileSystem.h"


int numFCB, numBLOCK;         //全局变量，复制是计算需要的资源；
#define DirDepth 20            //子目录个数；
char* _virtualDiskAddr;   //虚拟磁盘的入口地址；
FCB* _emptyFCB;    //空FCB列表；
FCB* _lastFCB;    //空FCB列表的末尾；为了回收设计的；
BLOCK* _emptyBLOCK;    //空BLOCK列表；
BLOCK* _lastBLOCK;    //空BLOCK列表的末尾；为了回收设计的；
int _emptyBLOCK_Count;      //可用的BLOCK数量；
int _emptyFCB_Count;         //可用的FCB的数量；
FCB* _current; //当前目录；
string current_path;      //当前路径；
FCB* _root;   //指向root，实现绝对路径；

/**********************************************************************/
//磁盘管理声明；
/**********************************************************************/
//初始化虚拟磁盘；
//由于用户可能需要恢复之前的内存状态，所以不按照当下的初始化，提供if判断即可。
void sys_initDisk() {
    _virtualDiskAddr = (char *)malloc(_virtualDiskSize * sizeof(char));   //声请空间：char是一个字节，所以虚拟磁盘的大小是1M；

    //初始化盘块的位式图:'0'表示未被使用，'1’表示已经使用
    for (int i = 0; i < _blockCount; i++)
    {
        _virtualDiskAddr[i] = '0';
    }

    //存放位式图的空间已被占用；
    //useBlock += BitMap_size;
    for (int i = 0; i < _systemUsedBlock; i++)
    {
        _virtualDiskAddr[i] = '1';
    }

    //建立根root；
    _root = new(_virtualDiskAddr + _bitMapCount * _blockSize)FCB();
    strcpy_s(_root->_name, "root");
    _root->_parent = _root;
    _current = _root;
    current_path = "//";


    //把空的FCB链接起来；
    //char* _FCB_StartAllocation = _virtualDiskAddr + _bitMapCount * _blockSize + 64;    //64是root的FCB；
    _emptyFCB = new(_root + 1)FCB();
    _lastFCB = _emptyFCB;
    for (int i = 1; i < _FCB_count - 1; i++)            //还剩127个FCB，每个64B；
    {
        _lastFCB->_child = new(_lastFCB + 1)FCB();
        _lastFCB = _lastFCB->_child;
    }
    _emptyFCB_Count = _FCB_count - 1;

    //把空的BLOCK链接起来；
    _emptyBLOCK = new(_virtualDiskAddr + _systemUsedBlock * _blockSize)BLOCK();
    _emptyBLOCK->_blockID = _systemUsedBlock;        //128;
    _lastBLOCK = _emptyBLOCK;
    for (int i = 1; i < _BLOCK_count; i++)
    {
        _lastBLOCK->_nextBlock = new(_lastBLOCK + 1)BLOCK();
        _lastBLOCK = _lastBLOCK->_nextBlock;
        _lastBLOCK->_blockID = _systemUsedBlock + i;
    }
    _emptyBLOCK_Count = _BLOCK_count;
}


//给开发者提供FCB，供创建文件或目录时使用；
FCB* getFCB() {
    if (_FCB_count>0)
    {
        //说明有可用的FCB，那么只要返回_emptyFCB中的一个，并修改FCB这个链表；
        FCB* fcb = _emptyFCB;
        _emptyFCB = _emptyFCB->_child;
        fcb->_child = NULL;
        _emptyFCB_Count--;
        return fcb;
    }
    return NULL;         //用户调用文件系统lib时，需要判断是否为空，而提供输出信息；
}

//给开发者提供BLOCK，供写入文件时使用；
BLOCK* getBLOCK(int need_BLOCK_Num) {
    if (_BLOCK_count >= need_BLOCK_Num)
    {
        BLOCK* block = _emptyBLOCK;
        for (int i = 0; i < need_BLOCK_Num - 1; i++)
        {
            _emptyBLOCK = _emptyBLOCK->_nextBlock;
        }
        BLOCK* tmp = _emptyBLOCK;
        _emptyBLOCK = _emptyBLOCK->_nextBlock;
        tmp->_nextBlock = NULL;
        _emptyBLOCK_Count -= need_BLOCK_Num;
        return block;
    }
    return NULL;
}

//删除文件时，回收并清理BLOCK；
void recoverBLOCK(BLOCK* block) {
    if (block == NULL)
        return;
    _lastBLOCK->_nextBlock = block;
    _lastBLOCK = _lastBLOCK->_nextBlock;
    strcpy_s(_lastBLOCK->_content, "");
    while (_lastBLOCK != NULL)
    {
        _lastBLOCK->_blockLength = 0;
        _emptyBLOCK_Count++;
        _lastBLOCK = _lastBLOCK->_nextBlock;
    }
}

//删除文件/目录时，回收并清理FCB；
void recoverFCB(FCB* fcb) {
    _lastFCB->_child = fcb;
    _lastFCB = _lastFCB->_child;
    recoverBLOCK(_lastFCB->_block);
    strcpy_s(_lastFCB->_name, "");
    _lastFCB->_type = 1;
    _lastFCB->_size = 0;

    _lastFCB->_block = NULL;
    _lastFCB->_parent = NULL;
    _lastFCB->_brother = NULL;
    _lastFCB->_child = NULL;

    _emptyFCB_Count++;
}


//在当前目录下搜索FCB；
FCB* returnSonFCB(FCB* currentDir, string name, int type) {
    FCB* tmp = currentDir->_child;
    while (tmp != NULL)
    {
        if (tmp->_name == name && tmp->_type == type)
        {
            //说明找到了；
            return tmp;
        }
        tmp = tmp->_brother;
    }
    return NULL;           //由开发者据此提供错误信息；
}

//在给定的目录中，判断是否存在，不存在返回NULL，存在返回这个FCB；
FCB* return_DIR_FCB(string DIR[], int count, bool isAbsolutePath)
{
    FCB* tmp;
    if (isAbsolutePath)
    {
        tmp = _root;
    }
    else
    {
        tmp = _current;
    }

    for (int i = 0; i < count; i++)
    {
        tmp = returnSonFCB(tmp, DIR[i], 1);
        if (tmp == NULL)
        {
            return NULL;           //说明当前目录下没有这个子目录；
        }
    }
    return tmp;
}

//可以实现路径对目录和文件的访问；
FCB* sys_returnFCB(string path_name, string &name, int type, FCB* &parent) {
    string* DIR = new string[DirDepth];                                     //string DIR[DirDepth];
    int i = 0;
    for (int j = 0; j < path_name.length(); j++) {
        if (path_name[j] == '/') {
            if (DIR[i] != "")
                i++;
            else continue;
        }
        else {
            DIR[i] += path_name[j];
        }
    }
    int count = 0;
    for (i = 0; i < 20; i++) {
        if (DIR[i] != "") {
            count++;
        }
        else {
            break;
        }
    }
    if (count == 0&&path_name[0]=='/')
    {
        //说明用户的路径;
        name = "root";
        parent = _root;
        return _root;
    }
    else
    {
        name = DIR[count - 1];      //这个为路径中的最后一个字段；
    }

    bool AbsolutePath = false;
    if (path_name[0] == '/') {
        AbsolutePath = true;
    }
    parent = return_DIR_FCB(DIR, count - 1, AbsolutePath);
    if (parent != NULL)
    {

        return returnSonFCB(parent, name, type);
    }
    else
    {
        return NULL;
    }
}



//删除目录和文件中有个相同的操作就是回收FCB，现抽取出来；
void delete_dirOrFile(FCB* deleting) {

    //因为要删除这个文件/目录，所以都需要把这个节点从目录树中摘除；

    FCB* tmp = deleting->_parent;
    if (tmp->_child == deleting) {
        tmp->_child = deleting->_brother;
    }
    else
    {
        tmp = tmp->_child;
        while (tmp->_brother != deleting)
        {
            tmp = tmp->_brother;
        }
        //要删除的节点一定存在；
        tmp->_brother = deleting->_brother;
    }

    //回收FCB；
    if (deleting->_type == 1)
    {
        //删除的是目录；
        //先删除这个目录中的内容；
        //删除这个目录中时，需要先删除brother；
        while (deleting->_child != NULL)
        {
            delete_dirOrFile(deleting->_child);
        }
        recoverFCB(deleting);
    }
    else
    {
        //是文件；直接回收就好；
        recoverFCB(deleting);
    }
}


//求要复制的，需要多少系统资源，FCB个数，BLOCK个数
void num_dirOrFile(FCB* adding) {

    numFCB++;

    FCB* tmp;
    tmp = adding->_child;
    if (tmp == NULL)
    {
        //说明adding是个文件；
        //只需要计算adding的BLOCK；

        double needAddBLOCK;

        needAddBLOCK = adding->_size / _contentSize;
        int tmp = needAddBLOCK;

        if (needAddBLOCK == tmp)
        {
            //说明没损失，所以分配的个数为tmp;
            numBLOCK += tmp;
        }
        else
        {
            //精度损失；上取整；
            numBLOCK += tmp + 1;
        }
    }
    else
    {
        //是个目录；
        while (tmp->_brother != NULL)
        {
            //递归计算,先把兄弟算了；
            num_dirOrFile(tmp->_brother);
            tmp = tmp->_brother;
        }
        //再算孩子；
        num_dirOrFile(adding->_child);
    }
}

//复制,当是目录时，需要copy目录里的所有内容；
void add_dirOrFile(FCB* adding, FCB* destination) {

    //destination ，复制到的那个目录；adding，需要复制的内容；
    //因为要增加这个文件/目录，所以都需要把这个节点复制一份移入目录树中；
    FCB* copy_adding = getFCB();
    strcpy_s(copy_adding->_name, adding->_name);
    //copy_adding->_spare = adding->_spare;
    copy_adding->_type = adding->_type;
    copy_adding->_size = adding->_size;
    copy_adding->_block = adding->_block;

    //下面两行实现了移入目录树中；
    copy_adding->_parent = destination;
    copy_adding->_brother = destination->_child;
    destination->_child = copy_adding;

    //这是区分目录和文件的关键；
    copy_adding->_child = NULL;
    //这里必须断掉brother;
    //copy_adding->_child->_brother = NULL;


    FCB* tmp;
    tmp = adding->_child;

    if (tmp == NULL)
    {
        //说明adding是个文件；
        //则需要add声请BLOCK，存放内容；

        //该文件有内容，需要增添BLOCK；

        //需要添加几个；
        double needAddBLOCK;

        needAddBLOCK = copy_adding->_size / _contentSize;
        if (needAddBLOCK==0)
        {
            //不需要BLOCK；
        }
        else
        {
            int tmp = needAddBLOCK;

            BLOCK* contentFirstAdrr;   //分配的BLOCK的首地址；

            if (needAddBLOCK == tmp)
            {
                //说明没损失，所以分配的个数为tmp;
                contentFirstAdrr = getBLOCK(tmp);

            }
            else
            {
                //精度损失；上取整；
                contentFirstAdrr = getBLOCK(tmp + 1);
            }

            copy_adding->_block = contentFirstAdrr;

            //指向adding的BLOCK；
            BLOCK* tmp_addingBLOCK;
            tmp_addingBLOCK = adding->_block;

            //先处理整数内容的BLOCK；
            for (int i = 0; i < tmp - 1; i++)
            {
                strcpy_s(contentFirstAdrr->_content, ((string)(tmp_addingBLOCK->_content)).substr(i*_contentSize, _contentSize).c_str());
                contentFirstAdrr->_blockLength = _contentSize;
                contentFirstAdrr = contentFirstAdrr->_nextBlock;
                tmp_addingBLOCK = tmp_addingBLOCK->_nextBlock;
            }
            //存取剩余内容；
            contentFirstAdrr->_blockLength = tmp_addingBLOCK->_blockLength;
            strcpy_s(contentFirstAdrr->_content, ((string)(tmp_addingBLOCK->_content)).substr(0, contentFirstAdrr->_blockLength).c_str());
        }
    }
    else
    {
        //先拷贝child;
        add_dirOrFile(tmp, copy_adding);
        tmp = tmp->_brother;
        while (tmp != NULL)
        {
            //本质是拷贝文件，所以文件/目录实现方式相同；
            add_dirOrFile(tmp, copy_adding);
            tmp = tmp->_brother;
        }
    }

}

/**********************************************************************/
//目录管理声明；
/**********************************************************************/


//查看当前目录内容；
//查看当前目录内容；扩展：实现查看任意路径下的内容；
StringList* sys_dir(string dirPath) {

    const int dir_type = 1;            //一定是目录；

    //需要知道用户想查看的目录存不存在；
    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想查看的目录是否存在；
    string dirName = "";             //用户想要查看的目录的名字；在下面函数获得；
    FCB* tmp;
    if (dirPath != "")
        tmp = sys_returnFCB(dirPath, dirName, dir_type, parentDir);       //获得要查看的目录；
    else tmp = _current;
    StringList *dir_content = new StringList();
    StringList *tmp_dirContent;

    if (tmp != NULL) {
        //要查看的目录存在；
        //用StringList 存取需要打印的信息；

        tmp_dirContent = dir_content;
        tmp = tmp->_child;

        while (tmp != NULL)
        {
            if (tmp->_type == 1)
            {
                //cout << "Dir:	" << tmp->name << endl;
                tmp_dirContent->content += "Dir:	" + (string)tmp->_name + "\n";
                tmp_dirContent->next = new StringList();
                tmp_dirContent = tmp_dirContent->next;


            }
            else if (tmp->_type == 0)
            {
                //cout << "File:	" << tmp->name << endl;
                tmp_dirContent->content += "File:	" + (string)tmp->_name + "\n";
                tmp_dirContent->next = new StringList();
                tmp_dirContent = tmp_dirContent->next;
            }
            if (tmp->_brother != NULL)
            {
                tmp = tmp->_brother;
            }
            else
            {
                tmp_dirContent->next = NULL;
                return dir_content;
            }
        }
        //cout << "当前目录为空" << endl;
        dir_content->content += "当前目录为空！\n";
        dir_content->next = NULL;
        return dir_content;
    }
    else
    {

        if (parentDir == NULL)
        {
            dir_content->content = "该目录的父目录不存在！\n";
            dir_content->next = NULL;
            return dir_content;
        }
        else
        {
            //说明想要删除的目录不存在；
            dir_content->content = "该目录不存在！\n";
            dir_content->next = NULL;
            return dir_content;
        }
    }
}

//创建目录；
int sys_mkdir(string dirPath) {

    const int dir_type = 1;

    //需要知道用户想创建的目录存不存在；
    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想创建的目录是否存在；
    string dirName = "";             //用户想要创建的目录的名字；在下面函数获得；

    if (sys_returnFCB(dirPath, dirName, dir_type, parentDir) == NULL) {
        //说明想要创建的目录不存在，继续判断想在哪个目录创建，那个目录是否存在
        if (parentDir != NULL)
        {
            //说明想在那个目录创建是合法的；
            //内核分配FCB；
            FCB* dirByUser;
            dirByUser = getFCB();     //返回给开发者一个FCB；待开发者使用；
            strcpy_s(dirByUser->_name, dirName.c_str());
            dirByUser->_parent = parentDir;
            dirByUser->_brother = parentDir->_child;
            parentDir->_child = dirByUser;

            //目录创建成功；返回1；提示开发者，向用户显示成功信息；
            return 1;

        }
        else
        {
            //提示开发者，由开发者告知用户，想要创建的那个目录所在的目录不存在，应该先创建它的父目录；
            return -1;
        }
    }
    else
    {
        //提示开发者，由开发者告知用户，想要创建的目录已存在，不允许创建；
        return -2;
    }
}

//删除目录;
int sys_rmdir(string dirPath) {
    const int dir_type = 1;

    //需要知道用户想删除的目录存不存在；
    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想删除的目录是否存在；
    string dirName = "";             //用户想要删除的目录的名字；在下面函数获得；

    FCB* tmp_deleteDir; //指向要删除的目录；
    tmp_deleteDir = sys_returnFCB(dirPath, dirName, dir_type, parentDir);

    if (tmp_deleteDir != NULL) {
        //说明想要删除的目录存在；
        delete_dirOrFile(tmp_deleteDir);
        return 1;                          //提示开发者，由开发者告知用户，删除目录成功；
    }
    else
    {
        if (parentDir == NULL)
        {
            return -1;          //提示开发者，想要删除的目录的父目录不存在；
        }
        else
        {
            //说明想要删除的目录不存在；
            return -2;               //提示开发者，由开发者告知用户，删除的目录不存在；
        }
    }

}

//创建文件；
int sys_create_file(string filePath) {
    const int file_type = 0;

    //需要知道用户想创建的文件存不存在；
    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想创建的文件是否存在；
    string fileName = "";             //用户想要创建的文件的名字；在下面函数获得；

    if (sys_returnFCB(filePath, fileName, file_type, parentDir) == NULL) {
        //说明想要创建的文件不存在，继续判断想在哪个目录创建，那个目录是否存在
        if (parentDir != NULL)
        {
            //说明想在那个目录创建是合法的；
            //内核分配FCB；
            FCB* fileByUser;
            fileByUser = getFCB();     //返回给开发者一个FCB；待开发者使用；
            strcpy_s(fileByUser->_name, fileName.c_str());
            fileByUser->_parent = parentDir;
            fileByUser->_brother = parentDir->_child;
            fileByUser->_type = 0;
            parentDir->_child = fileByUser;

            //文件创建成功；返回1；提示开发者，向用户显示成功信息；
            return 1;

        }
        else
        {
            //提示开发者，由开发者告知用户，想要创建的那个文件所在的目录不存在，应该先创建它的父目录；
            return -1;
        }
    }
    else
    {
        //提示开发者，由开发者告知用户，想要创建的文件已存在，不允许创建；
        return -2;
    }
}

//删除文件；
int sys_delete_file(string filePath) {

    const int file_type = 0;

    //需要知道用户想删除的文件存不存在；
    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想删除的文件是否存在；
    string fileName = "";             //用户想要删除的文件的名字；在下面函数获得；

    FCB* tmp_deleteFile; //指向要删除的文件；
    tmp_deleteFile = sys_returnFCB(filePath, fileName, file_type, parentDir);

    if (tmp_deleteFile != NULL) {
        //说明想要删除的文件存在
        delete_dirOrFile(tmp_deleteFile);
        return 1;                          //提示开发者，由开发者告知用户，删除文件成功；
    }
    else
    {
        //说明想要删除的文件不存在；
        return -1;               //提示开发者，由开发者告知用户，删除的文件不存在；
    }
}

//重命名；支持文件和目录
int sys_rename(string path, int type, string new_name) {
    //先判读用户给定的路径是否合法；即能否找到这个文件/目录；
    //type,有开发者询问用户后，获得；

    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想重命名的文件是否存在；
    string oldName = "";             //用户想要重命名的文件的名字；在下面函数获得；

    FCB* rename_FCB = sys_returnFCB(path, oldName, type, parentDir);

    if (rename_FCB != NULL)
    {
        //想重命名的文件/目录存在；

        //判断新名字是否在父目录中重复；
        if (returnSonFCB(parentDir, new_name, type) == NULL)
        {
            //说明，新名字没有重复；
            strcpy_s(rename_FCB->_name, new_name.c_str());
            return 1;        //提示开发者重命名成功；
        }
        else
        {
            //同名同类型已经存在，由于是遍历父目录下所有的孩子，可能是和自己重复，但拒绝重命名效果一样；
            return -1;              //提示开发者，存在同名同类型；
        }
    }
    else
    {
        //想重命名的文件不存在；
        if (parentDir == NULL)
        {
            //由于父目录不存在；
            return -2;       //提示开发者，父目录不存在；

        }
        else
        {
            return -3;         //提示开发者，想重命名的文件不存在；
        }
    }
}
//向文件写内容；
int sys_write_file(FCB* file, string content) {
    //文件是否存在，此时内核不考虑，由开发者处理；
    //所以，此时文件已经存在；

    //根据content分配block个数；
    double needUsedBlock;
    needUsedBlock = content.length() / _contentSize;
    int tmp = content.length() % _contentSize;

    BLOCK* contentFirstAdrr;   //分配的BLOCK的首地址；

    if (!tmp)
    {
        //说明没损失，所以分配的个数为tmp;
        contentFirstAdrr = getBLOCK(needUsedBlock);

        if (contentFirstAdrr == NULL)
        {
            return -1;              //提示开发者，所需要的BLOCK不够，写文件失败；
        }

    }
    else
    {
        //精度损失；上取整；
        needUsedBlock++;
        contentFirstAdrr = getBLOCK(needUsedBlock);

        if (contentFirstAdrr == NULL)
        {
            return -1;              //提示开发者，所需要的BLOCK不够，写文件失败；
        }
    }
    if (file->_block != NULL)
        recoverBLOCK(file->_block);
    file->_block = contentFirstAdrr;
    file->_size = content.length();

    //先处理整数内容的BLOCK；
    for (int i = 0; i < needUsedBlock - 1; i++)
    {
        strcpy_s(contentFirstAdrr->_content, content.substr(i*_contentSize, _contentSize).c_str());
        //content.substr(i*_contentSize, _contentSize).copy(contentFirstAdrr->_contentStart, content.substr(i*_contentSize, _contentSize).length());
        contentFirstAdrr->_blockLength = _contentSize;
        contentFirstAdrr = contentFirstAdrr->_nextBlock;
    }
    //存取剩余内容；
    contentFirstAdrr->_blockLength = tmp;
    strcpy_s(contentFirstAdrr->_content, content.substr((needUsedBlock - 1)*_contentSize, tmp).c_str());
    //content.substr((tmp - 1)*_contentSize, contentFirstAdrr->_blockLength).copy(contentFirstAdrr->_contentStart, content.substr((tmp - 1)*_contentSize, contentFirstAdrr->_blockLength).length());

    return 1;       //提示开发者，写文件成功；

}


//读取文件内容；
string sys_read_file(FCB* file) {
    //文件是否存在，此时内核不考虑，由开发者处理；
    //所以，此时文件已经存在；

    string content = "";
    BLOCK* tmp = file->_block;

    while (tmp->_nextBlock != NULL)
    {
        content += tmp->_content;
        tmp = tmp->_nextBlock;
    }
    content += ((string)tmp->_content).substr(0, tmp->_blockLength);

    content += "\n读取文件，成功！";
    return content;
}

//进入子目录；
int sys_cd(string path, string &name)
{
    const int dir_type = 1;

    FCB* parent = NULL;         //子目录的父目录；
    FCB* subDir;        //待查找的子目录；


    subDir = sys_returnFCB(path, name, dir_type, parent);

    //更新当前路径；
    if (path[0] == '/')
    {
        current_path = path;
    }
    else
    {
        if (_current == _root)
        {
            //直接加path;
            current_path += path;
        }
        else
        {
            current_path += '/' + path;
        }
    }


    if (subDir != NULL)
    {
        _current = subDir;

        return 1;   //提示开发者，进入目录，不用打印信息，显示效果就好；好好显示一下;
    }
    else
    {
        name = "想进入的目录不存在！";
        return -1;       //提示开发者，目录不存在；
    }
}

//返回上一级目录；
void sys_cdback() {
    current_path = current_path.substr(0, current_path.length() - strlen(_current->_name) );
    _current = _current->_parent;
}

//剪切；
int sys_cut(string oldPath, int type, string newPath) {
    //先判读用户给定的路径是否合法；即能否找到这个文件/目录；
    //type,newPath有开发者询问用户后，获得；

    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想剪切的文件是否存在；
    string cutFCB_name;

    FCB*  cut_FCB = sys_returnFCB(oldPath, cutFCB_name, type, parentDir);
    //如果要剪切的文件FCB不存在，那么返回文件不存在
    if (cut_FCB != NULL)
    {

        /************************************************************************/
        //可剪切粘贴；
        //首先判断；粘贴到的目录中，重复利用tmp，判断是否存在同名同类型文件/目录；
        //parentDir，cutFCB_name会根据path进行修改，可重复利用；
        //上面的oldPath是包括剪切的文件/目录的；这里的newPath是只需要提供要粘贴的目录就行；所以tmp此时是个父目录；
        FCB* DestinationDir_parentDir = NULL;
        FCB* DestinationDir = sys_returnFCB(newPath, cutFCB_name, 1, DestinationDir_parentDir);      //某个目录中；

        //判断目标目录是否存在；
        if (DestinationDir == NULL)
        {
            //目标目录不存在；则无法黏贴；
            return -4;              //提示开发者，目标目录不存在；
        }
        else
        {
            //目标目录存在，判读其中是否存在同名同类型的对象；
            //查找这个文件|目录的前驱；
            FCB* tmp;
            tmp = returnSonFCB(DestinationDir, cut_FCB->_name, cut_FCB->_type);
            if (tmp == NULL)
            {
                //说明不存在同名同类型的对象；可以黏贴；摘除之前目录树的结点
                if (parentDir->_child == cut_FCB) {
                    tmp = parentDir;
                    tmp->_child = cut_FCB->_brother;
                }
                else {
                    tmp = parentDir->_child;
                    while (tmp->_brother != cut_FCB)
                    {
                        tmp = tmp->_brother;
                    }
                    //一定能找到前驱；
                    //把要剪切的目录，从目录树中摘除；
                    tmp->_brother = cut_FCB->_brother;
                }
                //移入目标目录中；
                cut_FCB->_brother = DestinationDir->_child;
                DestinationDir->_child = cut_FCB;
                return 1;          //提示开发者，剪切粘贴成功；
            }
            else
            {
                return -1;       //提示开发者，该目录下存在同名同类型，不允许粘贴；
            }
        }

    }
    else
    {
        if (parentDir == NULL)
        {
            return -2;          //提示开发者，想要剪切的文件的父目录不存在；
        }
        else
        {
            //说明想要剪切的文件不存在；
            return -3;               //提示开发者，由开发者告知用户，剪切的文件不存在；
        }
    }
}

//复制；
int sys_copy(string oldPath, int type, string newPath) {
    //先判读用户给定的路径是否合法；即能否找到这个文件/目录；
    //type,有开发者询问用户后，获得；

    FCB*  parentDir = NULL;      //parentDir父目录，目前是不知道的，所以设置为NULL，通过下述过程，可以知道parentDir是否存在，以及想剪切的文件是否存在；
    string copyFCB_name;

    FCB*  copy_FCB = sys_returnFCB(oldPath, copyFCB_name, type, parentDir);

    if (copy_FCB != NULL)
    {
        //想要复制的存在；
        //复制时，获取要复制首地址即可，但粘贴要注意，拷贝一份和原来一样的，增加FCB开销，block开销；且目录的brother要置空；
        //_copyNode = copy_FCB;


        /**********************************************************/
        //进入复制粘贴；

        //先判断要复制到的那个目录中，是否存在同名同类型的文件；
        //copyFCB_name是纯粹打酱油的；
        FCB* DestinationDir = sys_returnFCB(newPath, copyFCB_name, 1, parentDir);      //某个目录中；
        if (DestinationDir==NULL)
        {
            //目标目录不存在；无法进行复制粘贴；
            return -5;           //提示开发者目标目录不存在；
        }
        else
        {
            //目标目录存在；
            //进入目标目录，判断是否存在同名同类型的对象；
            FCB* tmp;
            tmp = returnSonFCB(DestinationDir, copy_FCB->_name, copy_FCB->_type);
            if (tmp == NULL)
            {
                //用户想复制到的目录中没有同名同类型；
                //本质就是增加FCB，已经BlLOCK;
                //由于资源有限，所以需判断一个需要增加的FCB和BLOCK够不够；

                numFCB = 0; numBLOCK = 0;

                //调用计算系统资源；
                num_dirOrFile(copy_FCB);

                if (numFCB <= _emptyFCB_Count && numBLOCK <= _emptyBLOCK_Count)
                {
                    //说明系统资源够用；可以复制；
                    add_dirOrFile(copy_FCB, DestinationDir);
                    return 1;          //提示开发者， 复制粘贴成功；
                }
                else
                {
                    return -1;     //提示开发者，系统资源不够，无法进行复制；
                }

            }
            else
            {
                return -2;       //提示开发者，该目录下存在同名同类型，不允许粘贴；
            }
        }
    }
    else
    {
        if (parentDir == NULL)
        {
            return -3;          //提示开发者，想要复制的文件的父目录不存在；
        }
        else
        {
            //说明想要复制的文件不存在；
            return -4;               //提示开发者，由开发者告知用户，复制的文件不存在；
        }
    }
}