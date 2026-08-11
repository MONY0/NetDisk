#ifndef CKERNEL_H
#define CKERNEL_H
#include"maindialog.h"
#include <QObject>
#include"packdef.h"
#include"logindialog.h"
#include"common.h"
#include"csqlite.h"
#include<QList>
//#include"./INetMediator.h"
//核心处理类

//协议映射表
//类成员函数指针
class CKernel;
typedef void(CKernel::*PFun)(unsigned int lSendIP , char* buf , int nlen) ;


class INetMediator;
//单例 核心处理为什么要单例呢
/*
 * 1.构造 拷贝构造 析构 私有化
 * 2.提供静态的公有的获取对象方法
*/
//#define USE_SERVER 1
class CKernel : public QObject
{
    Q_OBJECT
private:
    explicit CKernel(QObject *parent = nullptr);
    explicit CKernel(const CKernel&kernel){}//拷贝构造
    ~CKernel();

    void loadIniFile();
    void setNetPackMap();
    void setSystemPath();
signals:
    void SIG_updateUploadFileProgress(int timestamp,int pos);
    void SIG_updateDownloadFileProgress(int timestamp,int pos);
public:
    static CKernel*GetInstance()
    {
        static CKernel kernel;
        return &kernel;
    }
private slots:
    //普通槽函数
    void slot_destroy();
    void slot_loginCommit(QString tel,QString password);
    void slot_registerCommit(QString tel,QString password,QString name);
    void slot_uploadFile(QString path,QString dir);
    void slot_uploadFolder(QString path,QString dir);
    void slot_dealUploadFileRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealFileContentRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_getCurFileList();
    void slot_addFolder(QString name,QString dir);
    void slot_dealGetFileInfoRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_changeDir(QString dir);

    //什么文件id的文件，什么目录下的文件 下载
    void slot_downloadFile(int fileid,QString dir);
    //什么文件id的文件，什么目录下的文件夹 下载
    void slot_downloadFolder(int fileifd ,QString dir);
    void slot_shareFile(QVector<int>fileidArray,QString dir);
    void slot_getShareByLink(int code,QString dir);
    void slot_deleteFile(QVector<int> fileidArray,QString dir);
    void slot_transferControl(int timestamp,int direction,bool pause);
    //网络槽函数
    void slot_dealClientData(unsigned int lSendIP , char* buf , int nlen);
    //登录回复处理
    void slot_dealLoginRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealRegisterRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealFileHeaderRq(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealFileContentRq(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealAddFolderRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealShareFileRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealQuickUploadRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealMyShareRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealGetShareRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_getMyShare();
    void slot_dealFolderHeadRq(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealDeleteFileRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealTransferCtrlRs(unsigned int lSendIP , char* buf , int nlen);
    void slot_dealContinueUploadRs(unsigned int lSendIP , char* buf , int nlen);
    void InitDatabase(int id);
    void slot_writeUploadTask(FileInfo &info);
    void slot_writeDwonloadTask(FileInfo &info);
    void slot_deleteUploadTask(FileInfo &info);
    void slot_deleteDownloadTask(FileInfo &info);
    void slot_GetUploadTask(QList<FileInfo> &infoList);
    void slot_GetDownloadTask(QList<FileInfo> &infoList);
    void slot_updateTaskProgress(FileInfo &info,int direction);

private:


    void SendData(char*buf,int len);
    void sendNextUploadBlock(FileInfo &info);
    void restoreCachedTasks();
    void sendContinueUpload(FileInfo &info);
    void sendContinueDownload(FileInfo &info);
private:
    MainDialog *m_mainDialog;
    LoginDialog* m_loginDialog;
    QString m_ip;
    QString m_port;

    QString m_name;
    int m_id;
    QString m_curDir;//网盘当前目录
    QString m_sysPath;//默认存储的系统路径（绝对路径 exe 同级
    INetMediator *m_tcpClient;
    CSqlite *m_sql;
    //key 时间戳 hhmmsszzz
    std::map<int,FileInfo>m_mapTimeStampToFileInfo;
    PFun m_netPackMap[_DEF_PACK_COUNT];
};

#endif // CKERNEL_H
