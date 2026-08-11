#ifndef COMMON_H
#define COMMON_H
#include<QString>
////////////////////文件信息/////////////////
struct FileInfo
{

    FileInfo():fileid(0),size(0),pos(0),timestamp(0),isPause(0),
      lastUiPos(0),pFile(nullptr){

    }

    int fileid;
    QString name;
    QString dir;//网盘目录
    QString time;
    int size; //int 32位 最大值 2G --假定网盘 文件都是2G最大
    QString md5;
    QString type;
    QString absolutePath;//文件本地决对地址

    int pos; //上传或下载到什么位置
    int timestamp;
    int isPause; //暂停  0 1
    int lastUiPos; //上一次刷新进度条的位置

    //文件指针
    FILE* pFile;

    //字节单位换算
    static QString getSize(int size) //得到KB  MB
    {
        QString res;
        int count=0;
        int tmp=size;
        while(tmp!=0)
        {
            tmp/=10024;
            if(tmp!=0)count++;
        }
        switch (count) {
        case 0:
            //arg 参数 第二个多宽 第三 进制 第四 不够宽度 缺省字符
            res=QString("0.%1KB").arg((int)(size%1024/1024.0*100) ,2,10,QChar('0')); ///0.KB
            break;
        case 1: //KB
            res=QString("%1.%2KB").arg(size/1024).arg((int)(size%1024/1024.0*100) ,2,10,QChar('0'));
            break;
        case 2: //MB
              res=QString("%1.%2MB").arg(size/1024/1024).arg((int)(size/1024%1024/1024.0*100) ,2,10,QChar('0'));
            break;
        case 3:
            break;
        default:
            break;
        }
        return res;
    }

};

















#endif // COMMON_H
