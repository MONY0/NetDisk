#include "ckernel.h"
#include<QDebug>
#include<QCoreApplication>
#include<QFileInfo>
#include<QSettings>//配置文件的类
#include"TcpClientMediator.h"
#include"TcpServerMediator.h"
#include<QMessageBox>
#include"md5.h"

#define NetMap(a) m_netPackMap[a-_DEF_PACK_BASE]
void CKernel::setNetPackMap()
{
    memset(m_netPackMap,0,sizeof(PFun)*_DEF_PACK_COUNT);
    //协议映射表 key 协议头偏移量 value 函数指针
    //通过 协议头找到对应的处理函数
    NetMap(_DEF_PACK_REGISTER_RS)=&CKernel::slot_dealRegisterRs;
    NetMap(_DEF_PACK_LOGIN_RS)=&CKernel::slot_dealLoginRs;
    NetMap(_DEF_PACK_UPLOAD_FILE_RS)=&CKernel::slot_dealUploadFileRs;
    NetMap(_DEF_PACK_FILE_CONTENT_RS)=&CKernel::slot_dealFileContentRs;
    NetMap(_DEF_PACK_GET_FILE_INFO_RS)=&CKernel::slot_dealGetFileInfoRs;
    NetMap(_DEF_PACK_FILE_HEADER_RQ)=&CKernel::slot_dealFileHeaderRq;
    NetMap(_DEF_PACK_FILE_CONTENT_RQ)=&CKernel::slot_dealFileContentRq;
    NetMap(_DEF_PACK_SHARE_FILE_RS)=&CKernel::slot_dealShareFileRs;
    NetMap(_DEF_PACK_ADD_FOLDER_RS)=&CKernel::slot_dealAddFolderRs;
    NetMap(_DEF_PACK_QUICK_UPLOAD_RS)=&CKernel::slot_dealQuickUploadRs;
    NetMap(_DEF_PACK_MY_SHARE_RS)=&CKernel::slot_dealMyShareRs;
    NetMap(_DEF_PACK_GET_SHARE_RS)=&CKernel::slot_dealGetShareRs;
    NetMap(_DEF_PACK_FOLDER_HEADER_RQ)=&CKernel::slot_dealFolderHeadRq;
    NetMap(_DEF_PACK_DELETE_FILE_RS)=&CKernel::slot_dealDeleteFileRs;
    NetMap(_DEF_PACK_TRANSFER_CTRL_RS)=&CKernel::slot_dealTransferCtrlRs;
    NetMap(_DEF_PACK_CONTINUE_UPLOAD_RS)=&CKernel::slot_dealContinueUploadRs;

}
#include<QDir>
void CKernel::setSystemPath()
{
    QString path = QDir(QCoreApplication::applicationDirPath())
                           .filePath("NetDisk");
    QDir dir;
    if(!dir.exists(path))
    {
        dir.mkdir(path);//只能创建一次
    }
    //默认路径
    m_sysPath=path;
}

static QString escapeSqlValue(QString value)
{
    return value.replace("'","''");
}

void CKernel::InitDatabase(int id)
{
    delete m_sql;
    m_sql=new CSqlite;
    QString path=QCoreApplication::applicationDirPath()+"/database/";
    QDir dir;
    if(!dir.mkpath(path))
    {
        qDebug()<<"create database directory failed"<<path;
        delete m_sql;
        m_sql=nullptr;
        return;
    }
    path+=QString("%1.db").arg(id);
    if(!m_sql->ConnectSql(path))
    {
        delete m_sql;
        m_sql=nullptr;
        return;
    }

    const QString fields=
            "(timestamp integer primary key,f_id integer,f_name text,f_dir text,"
            "f_time text,f_size integer,f_md5 text,f_type text,f_absolutePath text,"
            "pos integer default 0,is_pause integer default 0);";
    m_sql->UpdateSql("create table if not exists t_upload"+fields);
    m_sql->UpdateSql("create table if not exists t_download"+fields);
    const QStringList tables=QStringList()<<"t_upload"<<"t_download";
    for(int t=0;t<tables.size();++t)
    {
        QStringList schema;
        m_sql->SelectSql(QString("pragma table_info(%1);").arg(tables.at(t)),6,schema);
        bool hasPos=false,hasPause=false;
        for(int i=0;i+5<schema.size();i+=6)
        {
            hasPos|=schema.at(i+1)=="pos";
            hasPause|=schema.at(i+1)=="is_pause";
        }
        if(!hasPos)m_sql->UpdateSql(QString("alter table %1 add column pos integer default 0;").arg(tables.at(t)));
        if(!hasPause)m_sql->UpdateSql(QString("alter table %1 add column is_pause integer default 0;").arg(tables.at(t)));
    }
}

void CKernel::slot_writeUploadTask(FileInfo &info)
{
    if(!m_sql)return;
    slot_deleteUploadTask(info);
    QString sql=QString("insert into t_upload values(%1,%2,'%3','%4','%5',%6,'%7','%8','%9',%10,%11);")
            .arg(info.timestamp).arg(info.fileid).arg(escapeSqlValue(info.name))
            .arg(escapeSqlValue(info.dir)).arg(escapeSqlValue(info.time)).arg(info.size)
            .arg(escapeSqlValue(info.md5)).arg(escapeSqlValue(info.type))
            .arg(escapeSqlValue(info.absolutePath)).arg(info.pos).arg(info.isPause);
    m_sql->UpdateSql(sql);
}

void CKernel::slot_writeDwonloadTask(FileInfo &info)
{
    if(!m_sql)return;
    slot_deleteDownloadTask(info);
    QString sql=QString("insert into t_download values(%1,%2,'%3','%4','%5',%6,'%7','%8','%9',%10,%11);")
            .arg(info.timestamp).arg(info.fileid).arg(escapeSqlValue(info.name))
            .arg(escapeSqlValue(info.dir)).arg(escapeSqlValue(info.time)).arg(info.size)
            .arg(escapeSqlValue(info.md5)).arg(escapeSqlValue(info.type))
            .arg(escapeSqlValue(info.absolutePath)).arg(info.pos).arg(info.isPause);
    m_sql->UpdateSql(sql);
}

void CKernel::slot_deleteUploadTask(FileInfo &info)
{
    if(m_sql)m_sql->UpdateSql(QString("delete from t_upload where timestamp=%1;").arg(info.timestamp));
}

void CKernel::slot_deleteDownloadTask(FileInfo &info)
{
    if(m_sql)m_sql->UpdateSql(QString("delete from t_download where timestamp=%1;").arg(info.timestamp));
}

static FileInfo taskFromValues(const QStringList &v,int i)
{
    FileInfo info;
    info.timestamp=v.at(i).toInt(); info.fileid=v.at(i+1).toInt();
    info.name=v.at(i+2); info.dir=v.at(i+3); info.time=v.at(i+4);
    info.size=v.at(i+5).toInt(); info.md5=v.at(i+6); info.type=v.at(i+7);
    info.absolutePath=v.at(i+8); info.pos=v.at(i+9).toInt();
    info.isPause=v.at(i+10).toInt();
    return info;
}

void CKernel::slot_GetUploadTask(QList<FileInfo> &list)
{
    list.clear(); if(!m_sql)return;
    QStringList values;
    if(!m_sql->SelectSql("select * from t_upload order by timestamp;",11,values))return;
    for(int i=0;i+10<values.size();i+=11)list<<taskFromValues(values,i);
}

void CKernel::slot_GetDownloadTask(QList<FileInfo> &list)
{
    list.clear(); if(!m_sql)return;
    QStringList values;
    if(!m_sql->SelectSql("select * from t_download order by timestamp;",11,values))return;
    for(int i=0;i+10<values.size();i+=11)list<<taskFromValues(values,i);
}

void CKernel::slot_updateTaskProgress(FileInfo &info,int direction)
{
    if(!m_sql)return;
    QString table=direction==transfer_upload ? "t_upload" : "t_download";
    m_sql->UpdateSql(QString("update %1 set pos=%2,is_pause=%3 where timestamp=%4;")
                     .arg(table).arg(info.pos).arg(info.isPause).arg(info.timestamp));
}

void CKernel::slot_changeDir(QString dir)
{
    m_curDir=dir;
    m_mainDialog->slot_deleteAllFileInfo();
    slot_getCurFileList();
}



//生成MD5函数
#define MD5_KEY "1234"
static std::string getMD5(QString val)
{
    QString str=QString("%1_%2").arg(val).arg(MD5_KEY);
    MD5 md(str.toStdString());
    qDebug()<<str<<"md5: "<<md.toString().c_str();
    return md.toString();
}
#include<QTextCodec>

// QString -> char* gb2312
void Utf8ToGB2312( char* gbbuf , int nlen ,QString& utf8)
{
    //转码的对象
    QTextCodec * gb2312code = QTextCodec::codecForName( "gb2312");
    //QByteArray char 类型数组的封装类 里面有很多关于转码 和 写IO的操作
    QByteArray ba = gb2312code->fromUnicode( utf8 );// Unicode -> 转码对象的字符集

    strcpy_s ( gbbuf , nlen , ba.data() );
}

// char* gb2312 --> QString utf8
QString GB2312ToUtf8( char* gbbuf )
{
    //转码的对象
    QTextCodec * gb2312code = QTextCodec::codecForName( "gb2312");
    //QByteArray char 类型数组的封装类 里面有很多关于转码 和 写IO的操作
    return gb2312code->toUnicode( gbbuf );// 转码对象的字符集 -> Unicode
}
//获取文件MD5
static std::string getFileMD5(QString path)
{
    //打开文件 读取文件内容 读到MD5类 生成MD5
    FILE* pFile=nullptr;
    //fopen 如果有中文 支持ANSI 编码 使用acsii码
    //path 里面是
    char buf[1000]="";
    Utf8ToGB2312(buf,1000,path);
    pFile=fopen(buf,"rb");
    if(!pFile)
    {
        qDebug()<<"文件MD5失败";
    }
    int len=0;
    MD5 md;
    do
    {
        len=fread(buf,1,1000,pFile);//缓冲区，一次读多少，读多少次，文件指针，返回读取成功的次数
        md.update(buf,len);
    }while(len>0);
    fclose(pFile);
     qDebug()<<"file md5"<<md.toString().c_str();
     return md.toString();
}
void CKernel::SendData(char *buf, int len)
{
    m_tcpClient->SendData(0,buf,len);
}
CKernel::CKernel(QObject *parent) : QObject(parent),m_id(0),m_curDir("/"),m_sql(nullptr)
{
    setNetPackMap();
    //加载配置文件
    loadIniFile();

    //设置默认路径
    setSystemPath();

    m_tcpClient=new TcpClientMediator;
    connect(m_tcpClient,SIGNAL(SIG_ReadyData(uint,char*,int)),this,SLOT(slot_dealClientData(uint,char*,int)));

    //
    m_tcpClient->OpenNet(m_ip.toStdString().c_str(),m_port.toInt());
    m_loginDialog=new LoginDialog;
    m_loginDialog->show();
    connect(m_loginDialog,SIGNAL(SIG_LoginCommit(QString,QString)),this,SLOT(slot_loginCommit(QString,QString)));
    connect(m_loginDialog,SIGNAL(SIG_RegisterCommit(QString ,QString,QString )),this,SLOT(slot_registerCommit(QString, QString, QString)));
    m_mainDialog=new MainDialog;

    //回收
    connect(m_mainDialog,SIGNAL(SIG_close()),this,SLOT(slot_destroy()));
    connect(m_mainDialog,SIGNAL(SIG_uploadFile(QString,QString)),this,SLOT(slot_uploadFile(QString,QString)));
    connect(this,SIGNAL(SIG_updateUploadFileProgress(int,int)),m_mainDialog,SLOT(slot_updateUploadFileProgree(int,int)));
    connect(m_mainDialog,SIGNAL(SIG_downloadFile(int,QString)),this,SLOT(slot_downloadFile(int,QString)));
    connect(m_mainDialog,SIGNAL(SIG_downloadFolder(int,QString)),this,SLOT(slot_downloadFolder(int,QString)));
    connect(this,SIGNAL(SIG_updateDownloadFileProgress(int,int)),m_mainDialog,SLOT(slot_updateDownloadFileProgree(int,int )));
    connect(m_mainDialog,SIGNAL(SIG_addFolder(QString,QString)),this,SLOT(slot_addFolder(QString,QString)));
    connect(m_mainDialog,SIGNAL(SIG_changeDir(QString)),this,SLOT(slot_changeDir(QString)));
    connect(m_mainDialog,SIGNAL(SIG_uploadFolder(QString,QString)),this,SLOT(slot_uploadFolder(QString,QString)));
    connect(m_mainDialog,&MainDialog::SIG_shareFile,
            this,&CKernel::slot_shareFile);
    connect(m_mainDialog,SIGNAL(SIG_getShareByLink(int,QString)),this,SLOT(slot_getShareByLink(int,QString)));
    connect(m_mainDialog,&MainDialog::SIG_deleteFile,
            this,&CKernel::slot_deleteFile);
    connect(m_mainDialog,&MainDialog::SIG_transferControl,
            this,&CKernel::slot_transferControl);
}

void CKernel::restoreCachedTasks()
{
    QList<FileInfo> uploads;
    QList<FileInfo> downloads;
    slot_GetUploadTask(uploads);
    slot_GetDownloadTask(downloads);

    for(int i=0;i<uploads.size();++i)
    {
        FileInfo info=uploads.at(i);
        if(!QFileInfo::exists(info.absolutePath))
        {
            slot_deleteUploadTask(info);
            continue;
        }
        char pathbuf[1000]="";
        QString path=info.absolutePath;
        Utf8ToGB2312(pathbuf,sizeof(pathbuf),path);
        info.pFile=fopen(pathbuf,"rb");
        if(!info.pFile)continue;
        info.pos=qBound(0,info.pos,info.size);
        info.lastUiPos=info.pos;
        fseek(info.pFile,info.pos,SEEK_SET);
        info.isPause=1;
        m_mapTimeStampToFileInfo[info.timestamp]=info;
        m_mainDialog->slot_insertUploadFile(m_mapTimeStampToFileInfo[info.timestamp]);
    }

    for(int i=0;i<downloads.size();++i)
    {
        FileInfo info=downloads.at(i);
        if(!QFileInfo::exists(info.absolutePath))
        {
            slot_deleteDownloadTask(info);
            continue;
        }
        // The local file length is authoritative because SQLite is checkpointed
        // every 256 KiB and may lag behind bytes already written to disk.
        info.pos=qMin(static_cast<int>(QFileInfo(info.absolutePath).size()),info.size);
        info.lastUiPos=info.pos;
        char pathbuf[1000]="";
        QString path=info.absolutePath;
        Utf8ToGB2312(pathbuf,sizeof(pathbuf),path);
        info.pFile=fopen(pathbuf,"rb+");
        if(!info.pFile)continue;
        fseek(info.pFile,info.pos,SEEK_SET);
        info.isPause=1;
        slot_updateTaskProgress(info,transfer_download);
        m_mapTimeStampToFileInfo[info.timestamp]=info;
        m_mainDialog->slot_insertDownloadFile(m_mapTimeStampToFileInfo[info.timestamp]);
    }
}

CKernel::~CKernel()
{
    delete m_sql;
    m_sql=nullptr;
}
void CKernel::loadIniFile()
{
    //默认值
    m_ip="192.168.79.132";
    m_port="8000";
    //获取exe目录
    QString path= QCoreApplication::applicationDirPath()+"/config.ini";
    //看文件是否存在 存在加载，不存在创建并且写入默认值
    QFileInfo info(path);
    if(info.exists())
    {
        //存在
        QSettings setting(path,QSettings::IniFormat);
        //打开组
        setting.beginGroup("net");
        QVariant strIp=setting.value("ip","");
        QVariant strPort=setting.value("port","");
        if(!strIp.toString().isEmpty())m_ip=strIp.toString();
        if(!strPort.toString().isEmpty())m_port=strPort.toString();
        //关闭组
        setting.endGroup();
    }
    else
    {
        //不存在
        QSettings setting(path,QSettings::IniFormat);//没有就创建
        //打开组
        setting.beginGroup("net");
        //设置
        setting.setValue("ip",m_ip);
        setting.setValue("port",m_port);
        //关闭组
        setting.endGroup();

    }
    qDebug()<<"ip:"<<m_ip<< " port:"<<m_port;
}

void CKernel::slot_destroy()
{
    qDebug()<<__func__;
    m_tcpClient->CloseNet();
    delete m_tcpClient;
    delete m_mainDialog;
    delete m_loginDialog;
}

void CKernel::slot_loginCommit(QString tel, QString password)
{
    STRU_LOGIN_RQ rq;
    strcpy(rq.tel,tel.toStdString().c_str());
    //strcpy(rq.password,password.toStdString().c_str());
    strcpy(rq.password,getMD5(password).c_str());
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_registerCommit(QString tel, QString password, QString name)
{
    STRU_REGISTER_RQ rq;
    strcpy(rq.tel,tel.toStdString().c_str());
    //strcpy(rq.password,password.toStdString().c_str());
    strcpy(rq.password,getMD5(password).c_str());
    //兼容中文
    std::string strName = name.toStdString();

    strcpy(rq.name,strName.c_str());

    SendData((char*)&rq,sizeof(rq));
}
#include<QFileInfo>
#include<QDateTime>
//上传文件的槽函数
void CKernel::slot_uploadFile(QString path, QString dir)
{
    QFileInfo qFileInfo(path);
    //文件信息存储
    FileInfo info;
    info.absolutePath=path;
    info.dir=dir;
    info.md5=QString::fromStdString(getFileMD5(path));
    info.name=qFileInfo.fileName();

    info.size=qFileInfo.size();
    info.time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    info.type="file";
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    //bug 修复 反复检查时间戳有没有
    while(m_mapTimeStampToFileInfo.count(timestamp)>0)
    {
        timestamp++;
    }
    info.timestamp=timestamp;
    char buf[1000];
    Utf8ToGB2312(buf,1000,path);
    info.pFile=fopen(buf,"rb");
    if(!info.pFile)
    {
        qDebug()<<"打开失败pFile";
        return;
    }
    qDebug()<<"时间戳"<<timestamp;
    //存储到map中 key 时间戳 value 文件信息
    m_mapTimeStampToFileInfo[timestamp]=info;
    //发上传文件请求
    STRU_UPLOAD_FILE_RQ rq;
    //兼容中文
    std::string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    std::string strName=info.name.toStdString();
    strcpy(rq.fileName,strName.c_str());
    strcpy(rq.fileType,"file");
    strcpy(rq.md5,info.md5.toStdString().c_str());

    rq.size=info.size;
    strcpy(rq.time,info.time.toStdString().c_str());
    rq.timestamp=timestamp;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));

}

void CKernel::slot_uploadFolder(QString path, QString dir)
{
     qDebug()<<__func__;
     //当前文件夹的处理 addFolder
     QFileInfo info(path);
     QDir dr(path);
     //qDebug()<<"folder :"<<info.fileName()<<"dir:"<<dir;
     slot_addFolder(info.fileName(),dir);
     //获取文件夹下面的一层 所有文件的路径
     QFileInfoList lst=dr.entryInfoList();//获取路径下所有文件信息列表
     //遍历所有文件
     QString newDir=dir+info.fileName()+"/";
     for(int i=0;i<lst.size();i++)
     {
        QFileInfo file=lst.at(i);
        //如果是 .继续
        if(file.fileName()==".")continue;
        //如果是..继续
        if(file.fileName()=="..")continue;
        //如果是文件 uploadFile->路径 文件信息的绝对路径 传到什么目录
        if(file.isFile())
        {
            //qDebug()<<"file :"<<file.absoluteFilePath()<<"dir:"<<newDir;
            slot_uploadFile(file.absoluteFilePath(),newDir);
        }
        //如果是文件夹
        if(file.isDir())
        {
            slot_uploadFolder(file.absoluteFilePath(),newDir);
        }
     }
}

void CKernel::slot_dealUploadFileRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_UPLOAD_FILE_RS)))
    {
        qDebug()<<"invalid upload response"<<nlen;
        return;
    }
    //拆包
    STRU_UPLOAD_FILE_RS*rs=(STRU_UPLOAD_FILE_RS*)buf;

    //首先看结果是否为真
    if(rs->result!=net_result_success)
    {
        const bool exists=(rs->result==net_result_name_exists);
        if(m_mapTimeStampToFileInfo.count(rs->timestamp)>0)
        {
            FileInfo &info=m_mapTimeStampToFileInfo[rs->timestamp];
            slot_deleteUploadTask(info);
            if(info.pFile)fclose(info.pFile);
            m_mapTimeStampToFileInfo.erase(rs->timestamp);
        }
        QMessageBox::warning(m_mainDialog,QStringLiteral("提示"),
                             exists ? QStringLiteral("同一目录下已存在同名文件")
                                    : QStringLiteral("上传失败，请稍后重试"));
        return;
    }
    //为真
        //获取文件信息
        if(m_mapTimeStampToFileInfo.count(rs->timestamp)==0)
        {
            qDebug()<<"not found";
            return;
        }
        FileInfo& info=m_mapTimeStampToFileInfo[rs->timestamp];
        //更新fileid
        info.fileid=rs->fileid;
        slot_writeUploadTask(info);
        //插入上传信息到“上传中”的控件todo
        m_mainDialog->slot_insertUploadFile(info);
        //发送文件块请求
        if(!info.isPause)
        {
            sendNextUploadBlock(info);
        }
}

void CKernel::sendNextUploadBlock(FileInfo &info)
{
    if(info.isPause || !info.pFile || info.pos>=info.size)
    {
        return;
    }
    STRU_FILE_CONTENT_RQ rq;
    rq.fileid=info.fileid;
    rq.timestamp=info.timestamp;
    rq.userid=m_id;
    rq.len=static_cast<int>(fread(rq.content,1,_DEF_BUFFER,info.pFile));
    if(rq.len>0)
    {
        SendData((char*)&rq,sizeof(rq));
    }
}

void CKernel::slot_dealFileContentRs(unsigned int lSendIP, char *buf, int nlen)
{
    //拆包
    STRU_FILE_CONTENT_RS*rs=(STRU_FILE_CONTENT_RS*)buf;

    //找文件信息结构体
    if(m_mapTimeStampToFileInfo.count(rs->timestamp)==0)
    {
        qDebug()<<"file not found";
        return;
    }
    FileInfo &info=m_mapTimeStampToFileInfo[rs->timestamp];
    //结果
    if(!rs->result)
    {
        //结果假 跳回
        fseek(info.pFile,-1*(rs->len),SEEK_CUR);

    }
    else
    {
        //结果真 pos+len
        info.pos+=rs->len;
        if(info.pos>=info.size || info.pos%(256*1024)<rs->len)
            slot_updateTaskProgress(info,transfer_upload);
        //更新上传进度todo
        //方案1：写信号槽 考虑多线程
        if(info.pos>=info.size || info.pos-info.lastUiPos>=256*1024)
        {
            info.lastUiPos=info.pos;
            Q_EMIT SIG_updateUploadFileProgress(info.timestamp,info.pos);
        }
        //方案2：直接调用 一定是当前函数在主线程

        //判断是否结束
        if(info.pos>=info.size)
        {
            //是 关闭文件 回收 返回
            QString uploadedDir=info.dir;
            slot_deleteUploadTask(info);
            fclose(info.pFile);
            m_mapTimeStampToFileInfo.erase(rs->timestamp);

            //刷新
            m_mainDialog->slot_deleteAllFileInfo();
            slot_getCurFileList();
            return;
        }
    }

   //发文件块
    if(!info.isPause)
    {
        sendNextUploadBlock(info);
    }
}

void CKernel::slot_getCurFileList()
{
    // The response handler appends rows, so remove the previous snapshot first.
    m_mainDialog->slot_clearFileList();

    //向服务器发送获取当前目录文件列表
    STRU_FILE_INFO_RQ rq;
    rq.userid=m_id;
    //兼容中文
    std::string strDir=m_curDir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_addFolder(QString name, QString dir)
{
    //发包
    STRU_ADD_FOLDER_RQ rq;
    string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    string strName=name.toStdString();
    strcpy(rq.fileName,strName.c_str());
    string strTime=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString();
    strcpy(rq.time,strTime.c_str());
    rq.timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_dealGetFileInfoRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_FILE_INFO_RS)))return;
    //拆包
    STRU_FILE_INFO_RS*rs=(STRU_FILE_INFO_RS*)buf;
    if(!memchr(rs->dir,'\0',sizeof(rs->dir)))return;
    const int maxCount=(nlen-static_cast<int>(sizeof(STRU_FILE_INFO_RS))) /
            static_cast<int>(sizeof(STRU_FILE_INFO));
    if(rs->count<0 || rs->count>maxCount)return;
    if(m_curDir!=QString::fromStdString(rs->dir))return;

     m_mainDialog->slot_deleteAllFileInfo();
    int count=rs->count;
    //获取元素
    for(int i=0;i<count;i++)
    {
        FileInfo info;
        info.fileid=rs->fileinfo[i].fileid;
        info.type=QString::fromStdString(rs->fileinfo[i].filetype);
        info.name=QString::fromStdString(rs->fileinfo[i].name);
        info.size=rs->fileinfo[i].size;
        info.time=rs->fileinfo[i].time;

        //插入到控件
        m_mainDialog->slot_insertFileInfo(info);
    }



}


void CKernel::slot_downloadFile(int fileid, QString dir)
{
    //写请求
    STRU_DOWNLOAD_FILE_RQ rq;
    //兼容中文
    std::string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    rq.fileid=fileid;
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    while(m_mapTimeStampToFileInfo.count(timestamp)>0)
    {
        timestamp++;
    }
    rq.timestamp=timestamp;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_downloadFolder(int fileid, QString dir)
{
    STRU_DOWNLOAD_FOLDER_RQ rq;
    string strDir=dir.toStdString();
    strcpy(rq.dir,strDir.c_str());
    rq.fileid=fileid;
    int timestamp=QDateTime::currentDateTime().toString("hhmmsszzz").toInt();
    while(m_mapTimeStampToFileInfo.count(timestamp)>0)
    {
        timestamp++;
    }
    rq.timestamp=timestamp;
    rq.userid=m_id;

    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_shareFile(QVector<int> fileidArray, QString dir)
{
    if(fileidArray.isEmpty())
    {
        QMessageBox::information(m_mainDialog,"提示","请先勾选要分享的文件");
        return;
    }

    int packlen=sizeof(STRU_SHARE_FILE_RQ)+sizeof(int)*fileidArray.size();
    STRU_SHARE_FILE_RQ *rq=(STRU_SHARE_FILE_RQ*)malloc(packlen);
    rq->init();
    rq->itemCount=fileidArray.size();
    for(int i=0;i<fileidArray.size();i++)
    {
        rq->fileidArray[i]=fileidArray[i];
    }
    rq->userid=m_id;
    std::string strDir=dir.toStdString();
    strcpy(rq->dir,strDir.c_str());
    QString time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    strcpy(rq->shareTime,time.toStdString().c_str());
    SendData((char*)rq,packlen);
    free(rq);
}

void CKernel::slot_getShareByLink(int code, QString dir)
{
    STRU_GET_SHARE_RQ rq;
    string tmpDir=dir.toStdString();
    strcpy(rq.dir,tmpDir.c_str());
    rq.shareLink=code;
    string time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toStdString();
    strcpy(rq.time,time.c_str());
    rq.userid=m_id;

    SendData((char*)&rq,sizeof(rq));
}

void CKernel::slot_deleteFile(QVector<int> fileidArray, QString dir)
{
    if(fileidArray.isEmpty())
    {
        return;
    }

    const int packlen=sizeof(STRU_DELETE_FILE_RQ)
            +sizeof(int)*fileidArray.size();
    STRU_DELETE_FILE_RQ *rq=
            static_cast<STRU_DELETE_FILE_RQ*>(malloc(packlen));
    if(!rq)
    {
        QMessageBox::warning(m_mainDialog,"提示","创建删除请求失败");
        return;
    }

    rq->init();
    rq->userid=m_id;
    rq->fileCount=fileidArray.size();
    const QByteArray dirBytes=dir.toUtf8();
    strncpy(rq->dir,dirBytes.constData(),sizeof(rq->dir)-1);
    for(int i=0;i<fileidArray.size();++i)
    {
        rq->fileidArray[i]=fileidArray[i];
    }

    SendData(reinterpret_cast<char*>(rq),packlen);
    free(rq);
}

void CKernel::slot_transferControl(int timestamp, int direction, bool pause)
{
    std::map<int,FileInfo>::iterator it=m_mapTimeStampToFileInfo.find(timestamp);
    if(it==m_mapTimeStampToFileInfo.end())
    {
        return;
    }

    it->second.isPause=pause ? 1 : 0;
    slot_updateTaskProgress(it->second,direction);
    if(!pause)
    {
        if(direction==transfer_upload)sendContinueUpload(it->second);
        else if(direction==transfer_download)sendContinueDownload(it->second);
        return;
    }
    STRU_TRANSFER_CTRL_RQ rq;
    rq.userid=m_id;
    rq.fileid=it->second.fileid;
    rq.timestamp=timestamp;
    rq.direction=direction;
    rq.action=pause ? transfer_pause : transfer_resume;
    SendData(reinterpret_cast<char*>(&rq),sizeof(rq));
}

void CKernel::sendContinueUpload(FileInfo &info)
{
    STRU_CONTINUE_UPLOAD_RQ rq;
    rq.userid=m_id;
    rq.timestamp=info.timestamp;
    rq.fileid=info.fileid;
    QByteArray dir=info.dir.toUtf8();
    strncpy(rq.dir,dir.constData(),sizeof(rq.dir)-1);
    SendData(reinterpret_cast<char*>(&rq),sizeof(rq));
}

void CKernel::sendContinueDownload(FileInfo &info)
{
    STRU_CONTINUE_DOWNLOAD_RQ rq;
    rq.userid=m_id;
    rq.timestamp=info.timestamp;
    rq.fileid=info.fileid;
    rq.pos=info.pos;
    QByteArray dir=info.dir.toUtf8();
    strncpy(rq.dir,dir.constData(),sizeof(rq.dir)-1);
    SendData(reinterpret_cast<char*>(&rq),sizeof(rq));
}

void CKernel::slot_dealShareFileRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);

    if(!buf || nlen<static_cast<int>(sizeof(STRU_SHARE_FILE_RS)))
    {
        qDebug()<<"invalid share response"<<nlen;
        return;
    }

    STRU_SHARE_FILE_RS *rs=(STRU_SHARE_FILE_RS*)buf;
    if(rs->result)
    {
        QMessageBox::information(m_mainDialog,"提示","分享成功");
        //刷新
        slot_getMyShare();
    }
    else
    {
        QMessageBox::warning(m_mainDialog,"提示","分享失败");
    }
}
void CKernel::slot_getMyShare()
{
    STRU_MY_SHARE_RQ rq;
    rq.userid=m_id;
    SendData((char*)&rq,sizeof (rq));
}

void CKernel::slot_dealFolderHeadRq(unsigned int lSendIP, char *buf, int nlen)
{
    //拆包
    STRU_FOLDER_HEADER_RQ *rq=(STRU_FOLDER_HEADER_RQ*)buf;

    QString tmpDir=QString::fromStdString(rq->dir);
    QStringList dirList= tmpDir.split("/");//分隔
    QString pathsum=m_sysPath;

    for(QString & note:dirList)
    {
        if(!note.isEmpty())
        {
            pathsum+="/";
            pathsum+=note;

            QDir dir;
            if(!dir.exists(pathsum))
            {
                dir.mkdir(pathsum);
            }
        }
    }
    pathsum+="/";
    pathsum+=QString::fromStdString(rq->fileName);

    QDir dir;
    if(!dir.exists(pathsum))
    {
        dir.mkdir(pathsum);
    }
}
//客户端处理
void CKernel::slot_dealClientData(unsigned int lSendIP, char *buf, int nlen)
{
    //QString str=QString("来自客户端：%1").arg(QString::fromStdString(buf));
   // QMessageBox::about(NULL,"提示",str); //about 阻塞的 模态窗口（弹出来其他东西都不能点了）
    if(!buf || nlen<(int)sizeof(PackType))
    {
        delete []buf;
        return;
    }
    int type=*(int*)buf;
    qDebug()<<__func__;
    //通过协议头 拿到处理函数并执行
    if(type>=_DEF_PACK_BASE&&type<_DEF_PACK_BASE+_DEF_PACK_COUNT)
    {
       PFun pf= NetMap(type);
       if(pf)
       {
           (this->*pf)(lSendIP, buf, nlen);
       }
    }
    //回收空间
    delete []buf;
}




void CKernel::slot_dealLoginRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_LOGIN_RS)))return;
    qDebug()<<__func__;
    //拆包
    STRU_LOGIN_RS* rs=(STRU_LOGIN_RS*)buf;
    switch (rs->result) {
    case tel_not_exist:
        QMessageBox::about(m_loginDialog,"提示","电话号不存在");
        break;
    case password_error:
        QMessageBox::about(m_loginDialog,"提示","密码错误");
        break;
    case login_success:
        QMessageBox::about(m_loginDialog,"提示","登录成功");
        //前台
        m_loginDialog->hide();
        m_mainDialog->show();
        //后台
        m_name=rs->name;
        m_id=rs->userid;
        InitDatabase(m_id);
        restoreCachedTasks();

        //获取 根目录下面文件列表
        m_curDir="/";
        slot_getCurFileList();
        //刷新
        slot_getMyShare();
        break;
    case login_failed:
        QMessageBox::warning(m_loginDialog,"提示","登录服务暂时不可用");
        break;
    }
}

void CKernel::slot_dealRegisterRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_REGISTER_RS)))return;
    //拆包
    STRU_REGISTER_RS* rs=(STRU_REGISTER_RS*)buf;
    //根据不同的结果有不同的提示
    switch (rs->result) {
    case tel_is_exist:
        QMessageBox::about(m_loginDialog,"提示","电话号已存在");
        break;
    case name_is_exist:
        QMessageBox::about(m_loginDialog,"提示","昵称已存在");
        break;
    case register_success:
        QMessageBox::about(m_loginDialog,"提示","注册成功");
        break;
    case register_failed:
        QMessageBox::warning(m_loginDialog,"提示","注册失败，请稍后重试");
        break;
    }
}

void CKernel::slot_dealFileHeaderRq(unsigned int lSendIP, char *buf, int nlen)
{
    //拆包
    STRU_FILE_HEADER_RQ *rq=(STRU_FILE_HEADER_RQ*)buf;
    //创建文件信息结构体 赋值
    FileInfo info;
    //dir可能有很多层 todo

    QString tmpDir=QString::fromStdString(rq->dir);
    QStringList dirList= tmpDir.split("/");//分隔
    QString pathsum=m_sysPath;

    for(QString & note:dirList)
    {
        if(!note.isEmpty())
        {
            pathsum+="/";
            pathsum+=note;

            QDir dir;
            if(!dir.exists(pathsum))
            {
                dir.mkdir(pathsum);
            }
        }
    }
    info.dir=QString::fromStdString(rq->dir);
    info.name=QString::fromStdString(rq->fileName);
    info.absolutePath=QString("%1%2%3").arg(m_sysPath).arg(info.dir).arg(info.name);
    info.fileid=rq->fileid;
    info.md5=QString::fromStdString(rq->md5);

    info.size=rq->size;
    info.time=QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    info.timestamp=rq->timestamp;
    info.type="file";
    //打开文件
    char pathbuf[1000]="";
    Utf8ToGB2312(pathbuf,1000,info.absolutePath);
    info.pFile=fopen(pathbuf,"wb");
    if(!info.pFile)
    {
        qDebug()<<"file open fail";
        return;
    }
    //todo 保存下载信息到控件
    m_mainDialog->slot_insertDownloadFile(info);
    slot_writeDwonloadTask(info);
    //保存map里面
    m_mapTimeStampToFileInfo[rq->timestamp]=info;
    //写回复
    STRU_FILE_HEADER_RS rs;
    rs.fileid=rq->fileid;
    rs.result=1;
    rs.timestamp=rq->timestamp;
    rs.userid=m_id;
    SendData((char*)&rs,sizeof(rs));

}

void CKernel::slot_dealFileContentRq(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);

    if(!buf || nlen<static_cast<int>(sizeof(STRU_FILE_CONTENT_RQ)))
    {
        qDebug()<<"invalid file content packet"<<nlen;
        return;
    }

    //拆包
    STRU_FILE_CONTENT_RQ*rq=(STRU_FILE_CONTENT_RQ*)buf;

    if(rq->len<0 || rq->len>_DEF_BUFFER)
    {
        qDebug()<<"invalid file content length"<<rq->len;
        return;
    }

    //拿到文件信息结构体
    if(m_mapTimeStampToFileInfo.count(rq->timestamp)==0)return;

    //写文件
    FileInfo &info=m_mapTimeStampToFileInfo[rq->timestamp];
    if(!info.pFile)
    {
        qDebug()<<"download file is not open"<<rq->timestamp;
        return;
    }
    STRU_FILE_CONTENT_RS rs;
    int len=fwrite(rq->content,1,rq->len,info.pFile);
    if(len!=rq->len)
    {
        //不成功 跳回去
        rs.result=0;
        fseek(info.pFile,-1*len,SEEK_CUR);
    }
    else
    {
        //成功pos+=len
        rs.result=1;
        info.pos+=len;
        if(info.pos>=info.size || info.pos%(256*1024)<len)
            slot_updateTaskProgress(info,transfer_download);
        //更新进度 todo
        if(info.pos>=info.size || info.pos-info.lastUiPos>=256*1024)
        {
            info.lastUiPos=info.pos;
            Q_EMIT SIG_updateDownloadFileProgress(rq->timestamp,info.pos);
        }
        //要看 有没有到某尾 是否结束
        if(info.pos>=info.size)
        {
         //结束 关闭文件 回收
            slot_deleteDownloadTask(info);
            fclose(info.pFile);
            m_mapTimeStampToFileInfo.erase(rq->timestamp);
        }
    }
    //写回复
    rs.fileid=rq->fileid;
    rs.len=rq->len;
    rs.timestamp=rq->timestamp;
    rs.userid=m_id;
    //发送
    SendData((char*)&rs,sizeof(rs));
}

void CKernel::slot_dealAddFolderRs(unsigned int lSendIP, char *buf, int nlen)
{
    //拆包
    STRU_ADD_FOLDER_RS*rs=(STRU_ADD_FOLDER_RS*)buf;
    //判断是否成功
    if(rs->result!=1)return;
    //先删除原来的
    m_mainDialog->slot_deleteAllFileInfo();
    //更新文件列表
    slot_getCurFileList();
}

void CKernel::slot_dealQuickUploadRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_QUICK_UPLOAD_RS)))
    {
        qDebug()<<"invalid quick-upload response"<<nlen;
        return;
    }
    //拆包
    STRU_QUICK_UPLOAD_RS *rs=(STRU_QUICK_UPLOAD_RS*)buf;
    //获取文件信息
    if(m_mapTimeStampToFileInfo.count(rs->timestamp)==0)return;
    FileInfo &info=m_mapTimeStampToFileInfo[rs->timestamp];
    if(rs->result!=net_result_success)
    {
        slot_deleteUploadTask(info);
        if(info.pFile)fclose(info.pFile);
        m_mapTimeStampToFileInfo.erase(rs->timestamp);
        QMessageBox::warning(m_mainDialog,QStringLiteral("提示"),
                             rs->result==net_result_name_exists
                             ? QStringLiteral("同一目录下已存在同名文件")
                             : QStringLiteral("秒传失败，请稍后重试"));
        return;
    }
    slot_deleteUploadTask(info);
    //拿关闭文件
    if(info.pFile)fclose(info.pFile);
    //写入上传已完成信息
    m_mainDialog->slot_insertUploadComplete(info);
    //发送刷新文件列表
    if(m_curDir==info.dir)
    {
        m_mainDialog->slot_deleteAllFileInfo();
        slot_getCurFileList();
    }
    //删除节点
    m_mapTimeStampToFileInfo.erase(rs->timestamp);
}

void CKernel::slot_dealMyShareRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);

    if(!buf || nlen<static_cast<int>(sizeof(STRU_MY_SHARE_RS)))
    {
        qDebug()<<"invalid my-share response"<<nlen;
        return;
    }

    //拆包
    STRU_MY_SHARE_RS *rs=(STRU_MY_SHARE_RS*)buf;
    int count=rs->itemCount;
    const int headerSize=sizeof(STRU_MY_SHARE_RS);
    const int maxCount=(nlen-headerSize)/sizeof(STRU_MY_SHARE_FILE);
    if(count<0 || count>maxCount)
    {
        qDebug()<<"invalid my-share item count"<<count<<maxCount;
        return;
    }
    //遍历
    m_mainDialog->slot_deleteALLShareInfo();
    for(int i=0;i<count;i++)
    {
        m_mainDialog->slot_insertShareFileInfo(rs->items[i].name,rs->items[i].size,rs->items[i].time,rs->items[i].shareLink);
    }
}

void CKernel::slot_dealGetShareRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_GET_SHARE_RS)))
    {
        qDebug()<<"invalid get-share response"<<nlen;
        return;
    }
    //拆包
    STRU_GET_SHARE_RS *rs=(STRU_GET_SHARE_RS*)buf;
    //根据结果
    if(rs->result!=net_result_success)
    {
        QMessageBox::warning(m_mainDialog,QStringLiteral("提示"),
                             rs->result==net_result_name_exists
                             ? QStringLiteral("目标目录中已存在同名文件")
                             : QStringLiteral("保存分享失败，请稍后重试"));
        return;
    }
    else
    {
        //正确刷新
        if(QString::fromStdString(rs->dir)==m_curDir)
        {
            slot_getCurFileList();
        }
    }
}

void CKernel::slot_dealDeleteFileRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);

    if(!buf || nlen<static_cast<int>(sizeof(STRU_DELETE_FILE_RS)))
    {
        qDebug()<<"invalid delete response"<<nlen;
        return;
    }

    STRU_DELETE_FILE_RS *rs=
            reinterpret_cast<STRU_DELETE_FILE_RS*>(buf);
    const QString dir=QString::fromUtf8(rs->dir);
    if(rs->result)
    {
        QMessageBox::information(m_mainDialog,"提示","删除成功");
        if(dir==m_curDir)
        {
            slot_getCurFileList();
        }
    }
    else
    {
        QMessageBox::warning(m_mainDialog,"提示","删除失败，部分文件可能未删除");
        if(dir==m_curDir)
        {
            slot_getCurFileList();
        }
    }
}

void CKernel::slot_dealTransferCtrlRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_TRANSFER_CTRL_RS)))
    {
        return;
    }

    STRU_TRANSFER_CTRL_RS *rs=reinterpret_cast<STRU_TRANSFER_CTRL_RS*>(buf);
    std::map<int,FileInfo>::iterator it=m_mapTimeStampToFileInfo.find(rs->timestamp);
    if(it==m_mapTimeStampToFileInfo.end())
    {
        return;
    }
    if(!rs->result)
    {
        qDebug()<<"transfer control failed"<<rs->timestamp;
        return;
    }

    it->second.isPause=rs->action==transfer_pause ? 1 : 0;
    if(rs->direction==transfer_upload && rs->action==transfer_resume)
    {
        sendNextUploadBlock(it->second);
    }
}

void CKernel::slot_dealContinueUploadRs(unsigned int lSendIP, char *buf, int nlen)
{
    Q_UNUSED(lSendIP);
    if(!buf || nlen<static_cast<int>(sizeof(STRU_CONTINUE_UPLOAD_RS)))return;
    STRU_CONTINUE_UPLOAD_RS *rs=reinterpret_cast<STRU_CONTINUE_UPLOAD_RS*>(buf);
    std::map<int,FileInfo>::iterator it=m_mapTimeStampToFileInfo.find(rs->timestamp);
    if(it==m_mapTimeStampToFileInfo.end())return;
    FileInfo &info=it->second;
    if(!rs->result || rs->pos<0 || rs->pos>info.size)
    {
        info.isPause=1;
        slot_updateTaskProgress(info,transfer_upload);
        QMessageBox::warning(m_mainDialog,"提示","服务器无法恢复该上传任务");
        return;
    }
    if(fseek(info.pFile,rs->pos,SEEK_SET)!=0)
    {
        info.isPause=1;
        return;
    }
    info.pos=rs->pos;
    info.isPause=0;
    info.lastUiPos=info.pos;
    slot_updateTaskProgress(info,transfer_upload);
    Q_EMIT SIG_updateUploadFileProgress(info.timestamp,info.pos);
    if(info.pos<info.size)
    {
        sendNextUploadBlock(info);
    }
    else
    {
        slot_deleteUploadTask(info);
        if(info.pFile)fclose(info.pFile);
        m_mapTimeStampToFileInfo.erase(rs->timestamp);
        slot_getCurFileList();
    }
}




/*配置文件 什么位置 ？exe同级目录
 * 思路：看文件是否存在 存在加载，不存在创建并且写入默认值
 *
//.ini
//格式 [组名]
   key=value
   例如：
   [net]
   ip=192.168.5.198
   port=8000
*/

