#ifndef CLOGIC_H
#define CLOGIC_H

#include"TCPKernel.h"

class CLogic
{
public:
    CLogic( TcpKernel* pkernel )
    {
        m_pKernel = pkernel;
        m_sql = pkernel->m_sql;
        m_tcp = pkernel->m_tcp;
    }
public:
    //设置协议映射
    void setNetPackMap();
    int getNumber()
    {
        return 1000000000;
    }
    /************** 发送数据*********************/
    void SendData( sock_fd clientfd, char*szbuf, int nlen )
    {
        m_pKernel->SendData( clientfd ,szbuf , nlen );
    }
    /************** 网络处理 *********************/
    //注册
    void RegisterRq(sock_fd clientfd, char*szbuf, int nlen);
    //登录
    void LoginRq(sock_fd clientfd, char*szbuf, int nlen);

    void UploadFileRq(sock_fd clientfd,char *szbuf,int nlen);
    void FileContentRq(sock_fd clientfd,char *szbuf,int nlen);
    /*******************************************/
    void GetFileInfoRq(sock_fd clientfd,char *szbuf,int nlen);
    //下载文件请求
    void DownloadFileRq(sock_fd clientfd,char *szbuf,int nlen);
    //下载文件请求
    void DownloadFolderRq(sock_fd clientfd,char *szbuf,int nlen);
    void FileHeaderRs(sock_fd clientfd,char *szbuf,int nlen);
    void FileContentRs(sock_fd clientfd,char *szbuf,int nlen);
    void AddFolderRq(sock_fd clientfd,char *szbuf,int nlen);
    void ShareFileRq(sock_fd clientfd,char *szbuf,int nlen);
    void MyShareRq(sock_fd clientfd,char *szbuf,int nlen);
    void GetShareRq(sock_fd clientfd,char *szbuf,int nlen);
    void DeleteFileRq(sock_fd clientfd,char *szbuf,int nlen);
    void TransferCtrlRq(sock_fd clientfd,char *szbuf,int nlen);
    void ContinueDownloadRq(sock_fd clientfd,char *szbuf,int nlen);
    void ContinueUploadRq(sock_fd clientfd,char *szbuf,int nlen);
    bool ShareItem(int userid,int fileid,const string &dir,const string &time,int link);

    bool GetShareByFile( int userid, int fileid , string dir , string name , string time );
    bool GetShareByFolder( int userid , int fileid , string dir , string name , string time , int fromuserid , string fromdir );
    void DownloadFolder(int userid, int &timestamp, sock_fd clientfd, list<string> &lstRes);
    void DownloadFile(int userid, int &timestamp, sock_fd clientfd, list<string> &lstRes);
    bool DeleteItem(int userid, int fileid, const string &dir);
    bool DeleteFolderChildren(int userid, const string &dir);
    bool SendNextDownloadBlock(sock_fd clientfd, int userid, int timestamp, FileInfo *info);
    bool RebuildTransferInfo(int userid,int fileid,const string &dir,int direction,FileInfo *&info);
private:
    TcpKernel* m_pKernel;
    CMysql * m_sql;
    Block_Epoll_Net * m_tcp;
    MyMap<int,UserInfo*>m_mapIDTOUserInfo;
    MyMap<int64_t,FileInfo*>m_mapTimestampToFileInfo;
};

#endif // CLOGIC_H
