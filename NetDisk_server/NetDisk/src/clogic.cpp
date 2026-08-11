#include "clogic.h"

namespace
{
bool IsSafeVirtualDir(const char *dir)
{
    if(!dir)return false;
    const string value(dir);
    if(value.empty() || value[0]!='/' || value[value.size()-1]!='/' ||
       value.find('\\')!=string::npos)return false;
    if(value=="/")return true;

    size_t begin=1;
    while(begin<value.size())
    {
        const size_t slash=value.find('/',begin);
        if(slash==string::npos)return false;
        const string part=value.substr(begin,slash-begin);
        if(part.empty() || part=="." || part=="..")return false;
        begin=slash+1;
    }
    return true;
}

bool IsSafeEntryName(const char *name)
{
    if(!name)return false;
    const string value(name);
    if(value.empty() || value=="." || value==".." || value.size()>100 ||
       value.find_first_of("\\/:?*<>|\"")!=string::npos)return false;
    for(size_t i=0;i<value.size();++i)
        if(static_cast<unsigned char>(value[i])<32)return false;
    return true;
}

bool IsMd5(const char *md5)
{
    if(!md5 || strlen(md5)!=32)return false;
    for(size_t i=0;i<32;++i)
        if(!isxdigit(static_cast<unsigned char>(md5[i])))return false;
    return true;
}
}

void CLogic::setNetPackMap()
{
    NetPackMap(_DEF_PACK_REGISTER_RQ)    = &CLogic::RegisterRq;
    NetPackMap(_DEF_PACK_LOGIN_RQ)       = &CLogic::LoginRq;
    NetPackMap(_DEF_PACK_UPLOAD_FILE_RQ) = &CLogic::UploadFileRq;
    NetPackMap(_DEF_PACK_FILE_CONTENT_RQ)=&CLogic::FileContentRq;
    NetPackMap(_DEF_GET_PACK_FILE_INFO_RQ)=&CLogic::GetFileInfoRq;
    NetPackMap(_DEF_PACK_DOWNLOAD_FILE_RQ)=&CLogic::DownloadFileRq;
    NetPackMap(_DEF_PACK_FILE_HEADER_RS)=&CLogic::FileHeaderRs;
    NetPackMap(_DEF_PACK_FILE_CONTENT_RS)=&CLogic::FileContentRs;
    NetPackMap(_DEF_PACK_ADD_FOLDER_RQ)=&CLogic::AddFolderRq;
    NetPackMap(_DEF_PACK_SHARE_FILE_RQ)=&CLogic::ShareFileRq;
    NetPackMap(_DEF_PACK_MY_SHARE_RQ)=&CLogic::MyShareRq;
    NetPackMap(_DEF_PACK_GET_SHARE_RQ)=&CLogic::GetShareRq;
    NetPackMap(_DEF_PACK_DOWNLOAD_FOLDER_RQ)=&CLogic::DownloadFolderRq;
    NetPackMap(_DEF_PACK_DELETE_FILE_RQ)=&CLogic::DeleteFileRq;
    NetPackMap(_DEF_PACK_TRANSFER_CTRL_RQ)=&CLogic::TransferCtrlRq;
    NetPackMap(_DEF_PACK_CONTINUE_DOWNLOAD_RQ)=&CLogic::ContinueDownloadRq;
    NetPackMap(_DEF_PACK_CONTINUE_UPLOAD_RQ)=&CLogic::ContinueUploadRq;
}



#define _DEF_COUT_FUNC_    cout << "clientfd:"<< clientfd << __func__ << endl;

#define DEF_PATH "/home/coder/NetDisk/"
//注册
#define DEF_PATH "/home/coder/NetDisk/"
//注册
void CLogic::RegisterRq(sock_fd clientfd,char* szbuf,int nlen)
{
    _DEF_COUT_FUNC_
    STRU_REGISTER_RS rs;
    rs.result=register_failed;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_REGISTER_RQ)))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    STRU_REGISTER_RQ* rq = (STRU_REGISTER_RQ*)szbuf;
    if(!memchr(rq->tel,'\0',sizeof(rq->tel)) ||
       !memchr(rq->name,'\0',sizeof(rq->name)) ||
       !memchr(rq->password,'\0',sizeof(rq->password)))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    list<string> lstRes;

    // 1.校验手机号是否已存在
    bool queryRet=m_sql->SelectPrepared(
                "select u_tel from t_user where u_tel=?;",
                vector<MysqlParam>{MysqlParam::fromText(rq->tel)},1,lstRes);
    if(!queryRet)
    {
        std::cout<<"手机号查询失败"<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    if(lstRes.size() != 0)
    {
        rs.result = tel_is_exist;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return; // 手机号已存在直接返回，不再往下执行
    }

    // 2.校验昵称是否重复
    lstRes.clear();
    queryRet=m_sql->SelectPrepared(
                "select u_tel from t_user where u_name=?;",
                vector<MysqlParam>{MysqlParam::fromText(rq->name)},1,lstRes);
    if(!queryRet)
    {
        std::cout<<"昵称查询失败"<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    if(lstRes.size() != 0)
    {
        rs.result = name_is_exist;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    // 3.手机号、昵称都不重复，执行插入用户
    bool updateRet=m_sql->UpdatePrepared(
                "insert into t_user(u_tel,u_password,u_name) values (?,?,?);",
                vector<MysqlParam>{MysqlParam::fromText(rq->tel),
                                   MysqlParam::fromText(rq->password),
                                   MysqlParam::fromText(rq->name)});
    if(!updateRet)
    {
        std::cout<<"用户插入失败"<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    // 4.查询新注册用户u_id
    lstRes.clear();
    queryRet=m_sql->SelectPrepared(
                "select u_id from t_user where u_tel=?;",
                vector<MysqlParam>{MysqlParam::fromText(rq->tel)},1,lstRes);
    if(!queryRet || lstRes.empty())
    {
        std::cout<<"查询用户ID失败"<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    int uid = stoi(lstRes.front());
    char pathbuf[_MAX_PATH_SIZE] = "";
    sprintf(pathbuf,"%s%d", DEF_PATH, uid);

    // 创建用户独立存储目录
    umask(0);
    mkdir(pathbuf, 0777);

    // 发送注册成功应答包
    rs.result=register_success;
    SendData(clientfd,(char*)&rs,sizeof(rs));
}



//登录
void CLogic::LoginRq(sock_fd clientfd ,char* szbuf,int nlen)
{
    _DEF_COUT_FUNC_
    STRU_LOGIN_RS rs;
    rs.result=login_failed;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_LOGIN_RQ)))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    // 把接收的数据强转为登录请求结构体
    STRU_LOGIN_RQ*rq=(STRU_LOGIN_RQ*)szbuf;
    if(!memchr(rq->tel,'\0',sizeof(rq->tel)) ||
       !memchr(rq->password,'\0',sizeof(rq->password)))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    list<string> lstRes;

    // 根据手机号查询用户ID、密文密码、昵称三列数据
    bool queryRet=m_sql->SelectPrepared(
                "select u_id,u_password,u_name from t_user where u_tel=?;",
                vector<MysqlParam>{MysqlParam::fromText(rq->tel)},3,lstRes);
    if(!queryRet)
    {
        std::cout<<"登录查询失败"<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    // 无此手机号，返回手机号不存在
    if(lstRes.size()==0)
    {
        rs.result=tel_not_exist;
    }
    else
    {
        // 依次取出查询结果：用户ID、数据库存的加密密码、用户名
        int id=stoi(lstRes.front());
        lstRes.pop_front();
        string strPassword=lstRes.front();
        lstRes.pop_front();
        string strName=lstRes.front();
        lstRes.pop_front();

        // 比对客户端传来的加密密码和库内密文
        if(strcmp(strPassword.c_str(),rq->password)!=0)
        {
            rs.result=password_error;
        }
        else
        {
            // 登录成功，填充返回信息
            rs.result=login_success;
            rs.userid=id;
            strcpy(rs.name,strName.c_str());

            // 维护在线用户哈希表，记录该用户登录信息
            UserInfo*info=nullptr;
            if(!m_mapIDTOUserInfo.find(id,info))
            {
                info=new UserInfo;
            }
            else
            {
                //如果多设备登录，踢掉
            }
            info->name=strName;
            info->clientfd=clientfd;
            info->userid=id;
            m_mapIDTOUserInfo.insert(id,info);
        }
    }
    // 向客户端发送登录结果数据包
    SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::UploadFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;

    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_UPLOAD_FILE_RQ)))
    {
        STRU_UPLOAD_FILE_RS rs;
        rs.result=net_result_failed;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    //m_mapTimestampToFileInfo
    //1 拆包
    STRU_UPLOAD_FILE_RQ *rq=(STRU_UPLOAD_FILE_RQ*)szbuf;
    STRU_UPLOAD_FILE_RS uploadRs;
    uploadRs.timestamp=rq->timestamp;
    uploadRs.userid=rq->userid;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) ||
       !memchr(rq->fileName,'\0',sizeof(rq->fileName)) ||
       !memchr(rq->md5,'\0',sizeof(rq->md5)) ||
       !memchr(rq->fileType,'\0',sizeof(rq->fileType)) ||
       !memchr(rq->time,'\0',sizeof(rq->time)) ||
       rq->userid<=0 || rq->size<0 ||
       !IsSafeVirtualDir(rq->dir) || !IsSafeEntryName(rq->fileName) ||
       !IsMd5(rq->md5))
    {
        uploadRs.result=net_result_failed;
        SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
        return;
    }

    // The database unique index is the final guard, but checking first avoids
    // creating an orphan t_file row and a truncated local file on duplicates.
    list<string> duplicateResult;
    if(!m_sql->SelectPrepared(
           "select count(*) from t_user_file where u_id=? and f_dir=? and f_name=?;",
           vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                              MysqlParam::fromText(rq->dir),
                              MysqlParam::fromText(rq->fileName)},
           1,duplicateResult) || duplicateResult.empty())
    {
        uploadRs.result=net_result_failed;
        SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
        return;
    }
    if(stoi(duplicateResult.front())>0)
    {
        uploadRs.result=net_result_name_exists;
        SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
        return;
    }
    //2 查看是否秒传 todo
    {
        //查看 请求中的md5 对应的文件在数据库是否存在 md5 相等 状态位1
             //状态位0 表示 正在上传 客户端做校验不让其上传
             list<string> lstRes;
             bool res=m_sql->SelectPrepared(
                         "select f_id from t_file where f_MD5=? and f_state=1 limit 1;",
                         vector<MysqlParam>{MysqlParam::fromText(rq->md5)},1,lstRes);
             if( !res ){
                 cout<<"quick-upload select failed"<<endl;
                 uploadRs.result=net_result_failed;
                 SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
                 return;
             }
             //如果存在
             if( lstRes.size() != 0 ){
                 int fileid = stoi(lstRes.front());
                 lstRes.pop_front();
                 //写入用户文件关系
                 res=m_sql->UpdatePrepared(
                             "insert into t_user_file "
                             "(u_id,f_id,f_dir,f_name,f_uploadtime) values(?,?,?,?,?);",
                             vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                                MysqlParam::fromInt(fileid),
                                                MysqlParam::fromText(rq->dir),
                                                MysqlParam::fromText(rq->fileName),
                                                MysqlParam::fromText(rq->time)});
                 if( !res ){
                     cout<<"quick-upload relation insert failed"<<endl;
                     STRU_QUICK_UPLOAD_RS rs;
                     rs.result = net_result_failed;
                     rs.timestamp = rq->timestamp;
                     rs.userid = rq->userid;
                     SendData(clientfd,(char*)&rs,sizeof(rs));
                     return;
                 }
                 //写秒传回复
                 STRU_QUICK_UPLOAD_RS rs;
                 rs.result = net_result_success;
                 rs.timestamp = rq->timestamp;
                 rs.userid = rq->userid;
                 SendData( clientfd ,(char*)&rs ,sizeof(rs));
                 //返回
                 return;
             }
    }
    //3 不是秒传
     FileInfo *info=new FileInfo;
     char strpath[1000]="";
     sprintf(strpath,"%s%d%s%s",DEF_PATH,rq->userid,rq->dir,rq->md5);

     info->absolutePath =strpath;
     info->dir=rq->dir;
     //info->fid;
     info->md5=rq->md5;
     info->name=rq->fileName;
     info->size=rq->size;
     info->time=rq->time;
     info->type=rq->type;
     info->direction=transfer_upload;
    //4 文件信息创建  打开文件
     info->filefd=open(strpath,O_CREAT|O_WRONLY|O_TRUNC,00777);
     if(info->filefd<0)
     {
         std::cout<<"file open fialed"<<std::endl;
         delete info;
         uploadRs.result=net_result_failed;
         SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
         return;
     }
    //5 map 存储文件信息
     int64_t user_time=rq->userid*getNumber()+rq->timestamp;
     m_mapTimestampToFileInfo.insert(user_time,info);
     auto abortUpload = [&](bool deleteMetadata)
     {
         if(deleteMetadata)
         {
             m_sql->UpdatePrepared(
                         "delete from t_file where f_path=? and f_count=0 and f_state=0;",
                         vector<MysqlParam>{MysqlParam::fromText(strpath)});
         }
         if(info->filefd>=0)close(info->filefd);
         unlink(strpath);
         m_mapTimestampToFileInfo.erase(user_time);
         delete info;
         info=nullptr;
         uploadRs.result=net_result_failed;
         SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
     };
    //6 数据库记录
    //7 插入文件信息（引用计数 0 ，状态 0 -> 上传结束后改为 1 ）
     bool res=m_sql->UpdatePrepared(
                 "insert into t_file "
                 "(f_size,f_path,f_md5,f_count,f_state,f_type) values(?,?,?,0,0,'file');",
                 vector<MysqlParam>{MysqlParam::fromInt(rq->size),
                                    MysqlParam::fromText(strpath),
                                    MysqlParam::fromText(rq->md5)});
     if(!res)
     {
         cout<<"file metadata insert failed"<<endl;
         abortUpload(false);
         return;
     }
    //8 查文件 id
     list<string> lstRes;
     res=m_sql->SelectPrepared(
                 "select f_id from t_file where f_path=? and f_MD5=? order by f_id desc limit 1;",
                 vector<MysqlParam>{MysqlParam::fromText(strpath),
                                    MysqlParam::fromText(rq->md5)},1,lstRes);
     if( !res ){
         cout<<"file metadata id select failed"<<endl;
         abortUpload(true);
         return;
     }
     if(lstRes.size()>0)
     {
         info->fid=stoi(lstRes.front());
     }
     else
     {
         abortUpload(true);
         return;
     }
     lstRes.clear();
    //9 插入用户文件关系（由于触发器 引用计数 ->1 ）
     res=m_sql->UpdatePrepared(
                 "insert into t_user_file "
                 "(u_id,f_id,f_dir,f_name,f_uploadtime) values(?,?,?,?,?);",
                 vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                    MysqlParam::fromInt(info->fid),
                                    MysqlParam::fromText(rq->dir),
                                    MysqlParam::fromText(rq->fileName),
                                    MysqlParam::fromText(rq->time)});
     if( !res ){
         cout<<"user-file relation insert failed"<<endl;
         abortUpload(true);
         return;
     }

    //10 写回复包
     uploadRs.fileid=info->fid;
     uploadRs.result=net_result_success;
     SendData(clientfd,(char*)&uploadRs,sizeof(uploadRs));
}

void CLogic::FileContentRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_FILE_CONTENT_RQ)))return;
    STRU_FILE_CONTENT_RQ*rq=(STRU_FILE_CONTENT_RQ*)szbuf;
    if(rq->len<0 || rq->len>static_cast<int>(sizeof(rq->content)))return;
    int64_t user_time=rq->userid *getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info))
    {
        cout<<"file not found"<<endl;
        return;
    }
    STRU_FILE_CONTENT_RS rs;
    if(info->direction!=transfer_upload || info->fid!=rq->fileid ||
       info->isPause || info->pos>info->size-rq->len)
    {
        rs.result=0;
    }
    else
    {
        int written=0;
        while(written<rq->len)
        {
            const int len=write(info->filefd,rq->content+written,rq->len-written);
            if(len<0 && errno==EINTR)continue;
            if(len<=0)break;
            written+=len;
        }
        if(written!=rq->len)
        {
            if(written>0)lseek(info->filefd,-written,SEEK_CUR);
            rs.result=0;
        }
        else
        {
        rs.result=1;
        info->pos+=written;
        if(info->pos>=info->size)
        {
            close(info->filefd);
            m_mapTimestampToFileInfo.erase(user_time);
            delete info;
            info =nullptr;
            //geng xin shujuku
            bool res=m_sql->UpdatePrepared(
                        "update t_file set f_state=1 where f_id=?;",
                        vector<MysqlParam>{MysqlParam::fromInt(rq->fileid)});
            if(!res)
            {
                cout<<"file state update failed"<<endl;
            }
        }
        }
    }
    rs.fileid=rq->fileid;
    rs.len=rq->len;
    rs.timestamp=rq->timestamp;
    rs.userid=rq->userid;

    SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::GetFileInfoRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_FILE_INFO_RQ)))return;
    //拆包
    STRU_FILE_INFO_RQ*rq=(STRU_FILE_INFO_RQ*)szbuf;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) ||
       rq->userid<=0 || !IsSafeVirtualDir(rq->dir))return;
    //根据Id dir 查表（视图） 获取文件信息
    list<string>lstRes;

    bool res=m_sql->SelectPrepared(
                "select f_id,f_name,f_size,f_uploadtime,f_type "
                "from user_file_info where u_id=? and f_dir=? and f_state=1;",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                   MysqlParam::fromText(rq->dir)},5,lstRes);
    if(!res)
    {
        cout<<"file-list select failed"<<endl;
        return;
    }
    if(lstRes.size()%5!=0)return;
    //写回复包
    int count=lstRes.size()/5;
    int packlen=sizeof(STRU_FILE_INFO_RS)+count*sizeof(STRU_FILE_INFO);
    STRU_FILE_INFO_RS*rs=(STRU_FILE_INFO_RS*)malloc(packlen);

    rs->init();
    rs->count=count;
    strncpy(rs->dir,rq->dir,sizeof(rs->dir)-1);
    for(int i=0;i<count;++i)
    {
        int f_id=stoi(lstRes.front());lstRes.pop_front();
        string name=lstRes.front();lstRes.pop_front();
        int f_size=stoi(lstRes.front());lstRes.pop_front();
        string time=lstRes.front();lstRes.pop_front();
        string f_type=lstRes.front();lstRes.pop_front();
        rs->fileinfo[i].fileid=f_id;
        strcpy( rs->fileinfo[i].filetype,f_type.c_str());
        strcpy(rs->fileinfo[i].name,name.c_str());
        strcpy(rs->fileinfo[i].time,time.c_str());
        rs->fileinfo[i].size=f_size;
    }


        //send
    SendData(clientfd,(char*)rs,packlen);
    free(rs);

}

void CLogic::DownloadFileRq(sock_fd clientfd,char *szbuf,int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_DOWNLOAD_FILE_RQ)))return;
    //拆包
    STRU_DOWNLOAD_FILE_RQ *rq = (STRU_DOWNLOAD_FILE_RQ *)szbuf;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) || rq->userid<=0 ||
       rq->fileid<=0 || !IsSafeVirtualDir(rq->dir))return;

    //查数据库 查什么? f_name , f_path , f_MD5 , f_size
    list<string> lstRes;
    bool res=m_sql->SelectPrepared(
                "select f_name,f_path,f_MD5,f_size from user_file_info "
                "where u_id=? and f_dir=? and f_id=? limit 1;",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                   MysqlParam::fromText(rq->dir),
                                   MysqlParam::fromInt(rq->fileid)},4,lstRes);
    if( !res )
    {
        cout << "download file query failed" << endl;
        return;
    }
    //如果没有 返回
    if(lstRes.size()!=4)return;

    string strName = lstRes.front(); lstRes.pop_front();
    string strPath = lstRes.front(); lstRes.pop_front();
    string strMD5 = lstRes.front(); lstRes.pop_front();
    int size = stoi(lstRes.front()); lstRes.pop_front();

    //有 写文件信息
    FileInfo * info = new FileInfo;
    info->absolutePath = strPath;
    info->dir = rq->dir;
    info->fid = rq->fileid;

    info->md5 = strMD5;
    info->name = strName;
    info->size = size ;
    info->type = "file";
    info->direction = transfer_download;

    info->filefd = open(info->absolutePath.c_str(), O_RDONLY);
    if(info->filefd<0)
    {
        cout << "file open fail" << endl;
        delete info;
        return;
    }

    //key求出来
    int64_t user_time = rq->userid* getNumber() + rq->timestamp;
    //存到map里面
    m_mapTimestampToFileInfo.insert( user_time , info);

    //发送文件头请求
    STRU_FILE_HEADER_RQ headrq;
    strcpy(headrq.dir , rq->dir );
    headrq.fileid = rq->fileid ;
    strcpy(headrq.fileName , info->name.c_str() );
    strcpy(headrq.md5, info->md5.c_str() );
    strcpy(headrq.fileType , "file");
    headrq.size = info->size;
    headrq.timestamp = rq->timestamp;

    SendData( clientfd , (char*)&headrq , sizeof(headrq) );
}

void CLogic::DownloadFolderRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_DOWNLOAD_FOLDER_RQ)))return;
    //拆包
    STRU_DOWNLOAD_FOLDER_RQ* rq = (STRU_DOWNLOAD_FOLDER_RQ*)szbuf;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) || rq->userid<=0 ||
       rq->fileid<=0 || !IsSafeVirtualDir(rq->dir))return;

    //查数据库表 拿到信息
    //查询字段：f_type, f_id , f_name , f_path , f_MD5 , f_size , f_dir
    list<string> lstRes;
    bool res=m_sql->SelectPrepared(
                "select f_type,f_id,f_name,f_path,f_MD5,f_size,f_dir "
                "from user_file_info where u_id=? and f_dir=? and f_id=? limit 1;",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                   MysqlParam::fromText(rq->dir),
                                   MysqlParam::fromInt(rq->fileid)},7,lstRes);
    if( !res ){
        cout << "download folder query failed" <<endl;
        return;
    }

    if(lstRes.size()!=7)return;
    string type = lstRes.front(); lstRes.pop_front();
    if(type!="folder")return;
    int timestamp = rq->timestamp;

    //进入递归下载
    DownloadFolder( rq->userid , timestamp , clientfd , lstRes );
}


void CLogic::DownloadFolder( int userid ,int &timestamp , sock_fd clientfd, list<string> &lstRes )
{
    int fileid = stoi( lstRes.front() ); lstRes.pop_front();
    string strName = lstRes.front(); lstRes.pop_front();
    string strPath = lstRes.front(); lstRes.pop_front();
    string strMD5 = lstRes.front(); lstRes.pop_front();
    int size = stoi( lstRes.front() ); lstRes.pop_front();
    string dir = lstRes.front(); lstRes.pop_front();
    //string type = lstRes.front(); lstRes.pop_front();

    //发送创建文件夹请求
    STRU_FOLDER_HEADER_RQ rq;
    rq.timestamp = ++timestamp; // 时间戳处理
    strcpy( rq.dir , dir.c_str() );
    rq.fileid = fileid;
    strcpy( rq.fileName , strName.c_str() );
    SendData( clientfd , (char*)&rq , sizeof( rq ));

    //拼接路径
    string newdir = dir + strName + "/";

    //查询 newdir 下该用户所有文件信息
    list<string> newlstRes;
    bool res=m_sql->SelectPrepared(
                "select f_type,f_id,f_name,f_path,f_MD5,f_size,f_dir "
                "from user_file_info where u_id=? and f_dir=?;",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromText(newdir)},7,newlstRes);
    if( !res ){
        cout << "download folder children query failed" <<endl;
        return;
    }
    if(newlstRes.empty())return;
    if(newlstRes.size()%7!=0)return;
    while(newlstRes.size()!=0)
    {
    string type = newlstRes.front(); newlstRes.pop_front();
    if( type == "file"){
        //如果是文件 下载文件流程
        DownloadFile( userid , timestamp , clientfd, newlstRes );
    }else{
        //如果是文件夹 递归
        DownloadFolder( userid , timestamp , clientfd, newlstRes );
        }
    }
}

void CLogic::DownloadFile(int userid , int &timestamp , sock_fd clientfd , list<string> & lstRes )
{
    int fileid = stoi( lstRes.front() ); lstRes.pop_front();
    string strName = lstRes.front(); lstRes.pop_front();
    string strPath = lstRes.front(); lstRes.pop_front();
    string strMD5 = lstRes.front(); lstRes.pop_front();
    int size = stoi( lstRes.front() ); lstRes.pop_front();
    string dir = lstRes.front(); lstRes.pop_front();
    //string type = lstRes.front(); lstRes.pop_front();

    //封装文件信息
    FileInfo * info = new FileInfo;
    info->absolutePath = strPath;
    info->dir = dir;
    info->fid = fileid;
    info->md5 = strMD5;
    info->name = strName;
    info->size = size ;
    info->type = "file";
    info->direction = transfer_download;

    info->filefd = open( info->absolutePath.c_str() , O_RDONLY );
    if(info->filefd<0){
        cout << "file open fail" <<endl;
        delete info;
        return;
    }

    //生成唯一key
    int64_t user_time = userid* getNumber() + (++timestamp);
    //存入全局map，后续分片发送会通过这个key取出文件句柄
    m_mapTimestampToFileInfo.insert( user_time , info );

    //发送文件头部包
    STRU_FILE_HEADER_RQ headrq;
    strcpy( headrq.dir , dir.c_str() );
    headrq.fileid = fileid ;
    strcpy( headrq.fileName , info->name.c_str() );
    strcpy( headrq.md5 , info->md5.c_str() );
    strcpy( headrq.fileType , "file" );
    headrq.size = info->size;
    headrq.timestamp = timestamp ;

    SendData( clientfd , (char*)&headrq , sizeof(headrq) );
}

void CLogic::FileHeaderRs(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_FILE_HEADER_RS)))return;
    //拆包
    STRU_FILE_HEADER_RS*rs=(STRU_FILE_HEADER_RS*)szbuf;

    //拿到文件信息
    int64_t user_time=rs->userid*getNumber()+rs->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info))return;
    if(info->direction!=transfer_download || info->fid!=rs->fileid)return;

    if(rs->result!=net_result_success)
    {
        if(info->filefd>=0)close(info->filefd);
        m_mapTimestampToFileInfo.erase(user_time);
        delete info;
        return;
    }

    if(info->isPause)return;


    //发文件内容请求
    STRU_FILE_CONTENT_RQ rq;
    //读文件
    rq.len=read(info->filefd,rq.content,_DEF_BUFFER);
    if(rq.len<0){
        perror("read file error:");
        return;
    }
    rq.fileid=rs->fileid;
    rq.timestamp=rs->timestamp;
    rq.userid=rs->userid;
    SendData(clientfd,(char*)&rq,sizeof(rq));
}

void CLogic::FileContentRs(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_FILE_CONTENT_RS)))return;
    //拆包
    STRU_FILE_CONTENT_RS*rs=(STRU_FILE_CONTENT_RS*)szbuf;

    //文件信息结构
    int64_t user_time=rs->userid*getNumber()+rs->timestamp;
    FileInfo*info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info))return;
    if(info->direction!=transfer_download || info->fid!=rs->fileid ||
       rs->len<0 || rs->len>_DEF_BUFFER || info->pos>info->size-rs->len)return;
    //判断是否成功
    if(rs->result!=1)
    {
        //不成功 调回去
        lseek(info->filefd,-1*(rs->len),SEEK_CUR);
    }
    else
    {
        //成功 pos+=len
        info->pos+=rs->len;
        //判断是否结束
        if(info->pos>=info->size)
        {

            //是 关闭文件 回收 退出
            close(info->filefd);
            m_mapTimestampToFileInfo.erase(user_time);
            delete info;
            info=nullptr;
            return;
        }
    }
    if(info->isPause)return;
     //写文件内容请求
    STRU_FILE_CONTENT_RQ rq;
    //读文件
    rq.len =read(info->filefd,rq.content,_DEF_BUFFER);
    if(rq.len==0)return;
    if(rq.len<0)
    {
       perror("read fail");
       return;
    }
    rq.fileid=rs->fileid;
    rq.timestamp=rs->timestamp;
    rq.userid=rs->userid;


    //发送
    SendData(clientfd,(char*)&rq,sizeof(rq));
}

bool CLogic::SendNextDownloadBlock(sock_fd clientfd, int userid, int timestamp, FileInfo *info)
{
    if(!info || info->isPause || info->filefd<0 || info->pos>=info->size)
        return false;
    STRU_FILE_CONTENT_RQ rq;
    rq.len=read(info->filefd,rq.content,_DEF_BUFFER);
    if(rq.len<=0)
    {
        if(rq.len<0) perror("read file error");
        return false;
    }
    rq.fileid=info->fid;
    rq.timestamp=timestamp;
    rq.userid=userid;
    SendData(clientfd,(char*)&rq,sizeof(rq));
    return true;
}

void CLogic::TransferCtrlRq(sock_fd clientfd, char *szbuf, int nlen)
{
    STRU_TRANSFER_CTRL_RS rs;
    if(!szbuf || nlen<(int)sizeof(STRU_TRANSFER_CTRL_RQ))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    STRU_TRANSFER_CTRL_RQ *rq=(STRU_TRANSFER_CTRL_RQ*)szbuf;
    rs.timestamp=rq->timestamp;
    rs.direction=rq->direction;
    rs.action=rq->action;

    int64_t key=rq->userid*getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(key,info) ||
       (rq->direction!=transfer_upload && rq->direction!=transfer_download) ||
       (rq->action!=transfer_pause && rq->action!=transfer_resume) ||
       info->direction!=rq->direction)
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    info->isPause=(rq->action==transfer_pause) ? 1 : 0;
    rs.result=1;
    SendData(clientfd,(char*)&rs,sizeof(rs));

    // Download is server-driven, so resume must actively restart the pipeline.
    if(!info->isPause && info->direction==transfer_download)
        SendNextDownloadBlock(clientfd,rq->userid,rq->timestamp,info);
}

bool CLogic::RebuildTransferInfo(int userid, int fileid, const string &dir,
                                 int direction, FileInfo *&info)
{
    list<string> rows;
    if(!m_sql->SelectPrepared(
                "select f.f_path,f.f_size,f.f_MD5,uf.f_name "
                "from t_file f inner join t_user_file uf on f.f_id=uf.f_id "
                "where uf.u_id=? and uf.f_id=? and uf.f_dir=? limit 1;",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromInt(fileid),
                                   MysqlParam::fromText(dir)},4,rows) ||
       rows.size()!=4)return false;

    FileInfo *tmp=new FileInfo;
    tmp->absolutePath=rows.front(); rows.pop_front();
    tmp->size=stoi(rows.front()); rows.pop_front();
    tmp->md5=rows.front(); rows.pop_front();
    tmp->name=rows.front(); rows.pop_front();
    tmp->fid=fileid;
    tmp->dir=dir;
    tmp->type="file";
    tmp->direction=direction;
    int flags=direction==transfer_upload ? O_WRONLY : O_RDONLY;
    tmp->filefd=open(tmp->absolutePath.c_str(),flags);
    if(tmp->filefd<0)
    {
        delete tmp;
        return false;
    }
    if(direction==transfer_upload)
    {
        off_t end=lseek(tmp->filefd,0,SEEK_END);
        if(end<0 || end>tmp->size)
        {
            close(tmp->filefd); delete tmp; return false;
        }
        tmp->pos=(int)end;
    }
    info=tmp;
    return true;
}

void CLogic::ContinueDownloadRq(sock_fd clientfd, char *szbuf, int nlen)
{
    if(!szbuf || nlen<(int)sizeof(STRU_CONTINUE_DOWNLOAD_RQ))return;
    STRU_CONTINUE_DOWNLOAD_RQ *rq=(STRU_CONTINUE_DOWNLOAD_RQ*)szbuf;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) || rq->userid<=0 ||
       rq->fileid<=0 || rq->pos<0 || !IsSafeVirtualDir(rq->dir))return;
    int64_t key=rq->userid*getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(key,info))
    {
        if(!RebuildTransferInfo(rq->userid,rq->fileid,rq->dir,
                                transfer_download,info))return;
        m_mapTimestampToFileInfo.insert(key,info);
    }
    if(info->direction!=transfer_download || rq->pos>info->size)return;
    if(lseek(info->filefd,rq->pos,SEEK_SET)<0)return;
    info->pos=rq->pos;
    info->isPause=0;
    SendNextDownloadBlock(clientfd,rq->userid,rq->timestamp,info);
}

void CLogic::ContinueUploadRq(sock_fd clientfd, char *szbuf, int nlen)
{
    STRU_CONTINUE_UPLOAD_RS rs;
    if(!szbuf || nlen<(int)sizeof(STRU_CONTINUE_UPLOAD_RQ))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    STRU_CONTINUE_UPLOAD_RQ *rq=(STRU_CONTINUE_UPLOAD_RQ*)szbuf;
    rs.fileid=rq->fileid;
    rs.timestamp=rq->timestamp;
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) || rq->userid<=0 ||
       rq->fileid<=0 || !IsSafeVirtualDir(rq->dir))
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    int64_t key=rq->userid*getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(key,info))
    {
        if(!RebuildTransferInfo(rq->userid,rq->fileid,rq->dir,
                                transfer_upload,info))
        {
            SendData(clientfd,(char*)&rs,sizeof(rs));
            return;
        }
        m_mapTimestampToFileInfo.insert(key,info);
    }
    if(info->direction!=transfer_upload)
    {
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    info->isPause=0;
    rs.pos=info->pos;
    rs.result=1;
    if(info->pos>=info->size)
    {
        m_sql->UpdatePrepared("update t_file set f_state=1 where f_id=?;",
                              vector<MysqlParam>{MysqlParam::fromInt(info->fid)});
    }
    SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::AddFolderRq(sock_fd clientfd, char *szbuf, int nlen)
{
_DEF_COUT_FUNC_;

if(!szbuf || nlen<static_cast<int>(sizeof(STRU_ADD_FOLDER_RQ)))
{
    return;
}

//拆包
STRU_ADD_FOLDER_RQ *rq = (STRU_ADD_FOLDER_RQ *)szbuf;
STRU_ADD_FOLDER_RS rs;
rs.timestamp = rq->timestamp;
rs.userid = rq->userid;

if(!memchr(rq->fileName,'\0',sizeof(rq->fileName)) ||
        !memchr(rq->dir,'\0',sizeof(rq->dir)) ||
        !memchr(rq->time,'\0',sizeof(rq->time)) || rq->userid<=0 ||
        !IsSafeVirtualDir(rq->dir) || !IsSafeEntryName(rq->fileName))
{
    rs.result=0;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

const string folderName=rq->fileName;
bool onlyWhitespace=true;
for(size_t i=0;i<folderName.size();++i)
{
    if(!isspace(static_cast<unsigned char>(folderName[i])))
    {
        onlyWhitespace=false;
        break;
    }
}

if(folderName.empty() || folderName.size()>100 || onlyWhitespace ||
        folderName.find_first_of("\\/:?*<>|\"")!=string::npos)
{
    rs.result=0;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

//数据库写表 插入文件信息
//f_size,f_path,f_count,f_MD5,f_state,f_type
const string path=string(DEF_PATH)+to_string(rq->userid)+rq->dir+folderName;
list<string> duplicateRows;
if(!m_sql->SelectPrepared(
            "select count(*) from t_user_file where u_id=? and f_dir=? and f_name=?;",
            vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                               MysqlParam::fromText(rq->dir),
                               MysqlParam::fromText(folderName)},1,duplicateRows) ||
   duplicateRows.empty())
{
    rs.result=net_result_failed;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}
if(stoi(duplicateRows.front())>0)
{
    rs.result=net_result_name_exists;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

bool createdFolder=false;
bool success=m_sql->BeginTransaction();
if(success)
{
    success=m_sql->UpdatePrepared(
                "insert into t_file(f_size,f_path,f_count,f_MD5,f_state,f_type) "
                "values(0,?,0,'?',1,'folder');",
                vector<MysqlParam>{MysqlParam::fromText(path)});
}

//查询id
list<string> idRows;
if(success)
    success=m_sql->SelectPrepared("select last_insert_id();",
                                  vector<MysqlParam>(),1,idRows) && idRows.size()==1;

int id=0;
if(success)id=stoi(idRows.front());

//写入用户文件关系 -- 隐藏 触发器引用计数会+1
//u_id ,f_id ,f_dir ,f_name ,f_uploadtime
if(success)
{
    success=m_sql->UpdatePrepared(
                "insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) "
                "values(?,?,?,?,?);",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                   MysqlParam::fromInt(id),
                                   MysqlParam::fromText(rq->dir),
                                   MysqlParam::fromText(folderName),
                                   MysqlParam::fromText(rq->time)});
}

//创建目录
if(success)
{
    umask(0);
    createdFolder=(mkdir(path.c_str(),0777)==0);
    success=createdFolder;
}

if(success)
{
    if(!m_sql->CommitTransaction())
    {
        success=false;
    }
}
else m_sql->RollbackTransaction();

if(!success && createdFolder)rmdir(path.c_str());
rs.result=success ? net_result_success : net_result_failed;
SendData(clientfd,(char*)&rs,sizeof(rs));

//写回复
//发送
}

void CLogic::ShareFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
_DEF_COUT_FUNC_;

STRU_SHARE_FILE_RS rs;
if(!szbuf || nlen<static_cast<int>(sizeof(STRU_SHARE_FILE_RQ)))
{
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

//拆包
STRU_SHARE_FILE_RQ *rq = (STRU_SHARE_FILE_RQ *)szbuf;
const int payload=nlen-static_cast<int>(sizeof(STRU_SHARE_FILE_RQ));
const int maxCount=payload/static_cast<int>(sizeof(int));
if(!memchr(rq->dir,'\0',sizeof(rq->dir)) ||
   !memchr(rq->shareTime,'\0',sizeof(rq->shareTime)) ||
   rq->userid<=0 || !IsSafeVirtualDir(rq->dir) ||
   rq->itemCount<=0 || rq->itemCount>maxCount)
{
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

//随机生成分享链接
//分享码规则 9位分享码
int link = 0;
do{
    link = 1+ random()%9 ; // 随机出 1-9
    link *= 100000000;
    link += random() % 100000000;
    //去重 查链接是否已经存在
    list<string> lstRes;
    bool res=m_sql->SelectPrepared(
                "select s_link from t_user_file where s_link=? limit 1;",
                vector<MysqlParam>{MysqlParam::fromInt(link)},1,lstRes);
    if( !res ){
        cout << "share link query failed" <<endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    if( lstRes.size() > 0 ){
        link = 0;
    }
}while( link == 0 );

//遍历 所有文件，将其分享链接设置
bool success=m_sql->BeginTransaction();
for(int i=0;success && i<rq->itemCount;++i)
    success=ShareItem(rq->userid,rq->fileidArray[i],rq->dir,rq->shareTime,link);

if(success)
{
    if(!m_sql->CommitTransaction())
    {
        success=false;
    }
}
else m_sql->RollbackTransaction();

//写回复
rs.result=success ? net_result_success : net_result_failed;
SendData( clientfd , (char*)&rs , sizeof(rs) );
}

void CLogic::MyShareRq(sock_fd clientfd, char *szbuf, int nlen)
{
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_MY_SHARE_RQ)))return;
    //拆包
    STRU_MY_SHARE_RQ * rq = (STRU_MY_SHARE_RQ *)szbuf;

    //根据id 查询 获得分享文件列表
    //查的内容 f_name f_size s_linkTime s_link
    list<string> lst;
    bool res=m_sql->SelectPrepared(
                "select f_name,f_size,s_linkTime,s_link from user_file_info "
                "where u_id=? and s_link is not null and s_linkTime is not null;",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid)},4,lst);
    if(!res ){
        cout << "my-share query failed" << endl;
        return;
    }

    int count = lst.size();
    if(count%4!=0)return;
    count /= 4;
    const int packlen=sizeof(STRU_MY_SHARE_RS)+count*sizeof(STRU_MY_SHARE_FILE);

    //写回复
    STRU_MY_SHARE_RS *rs = (STRU_MY_SHARE_RS *)malloc(packlen);
    if(!rs)return;
    memset(rs,0,packlen);
    rs->init();
    rs->itemCount = count;
    for( int i = 0 ; i < count ; ++i){
        string name = lst.front(); lst.pop_front();
        int size = stoi( lst.front()); lst.pop_front();
        string time =lst.front(); lst.pop_front();
        int link = stoi(lst.front()); lst.pop_front();

        strncpy(rs->items[i].name,name.c_str(),sizeof(rs->items[i].name)-1);
        rs->items[i].size =size;
        strncpy(rs->items[i].time,time.c_str(),sizeof(rs->items[i].time)-1);
        rs->items[i].shareLink = link;
    }

    //发送
    SendData(clientfd,(char*)rs,packlen);
    free(rs);
}

void CLogic::GetShareRq(sock_fd clientfd, char *szbuf, int nlen)
{
_DEF_COUT_FUNC_;
if(!szbuf || nlen<static_cast<int>(sizeof(STRU_GET_SHARE_RQ)))
{
    STRU_GET_SHARE_RS rs;
    rs.result=net_result_failed;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}
//拆包
STRU_GET_SHARE_RQ * rq = (STRU_GET_SHARE_RQ *)szbuf;
if(!memchr(rq->dir,'\0',sizeof(rq->dir)) ||
   !memchr(rq->time,'\0',sizeof(rq->time)) || rq->userid<=0 ||
   rq->shareLink<=0 || !IsSafeVirtualDir(rq->dir))
{
    STRU_GET_SHARE_RS rs;
    rs.result=net_result_failed;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}
//根据分享码 查询到一系列文件
//查询属性：f_id  f_name f_dir(分享人的) f_type u_id(分享人的)
list<string> lst;
bool res=m_sql->SelectPrepared(
            "select f_id,f_name,f_dir,f_type,u_id from user_file_info where s_link=?;",
            vector<MysqlParam>{MysqlParam::fromInt(rq->shareLink)},5,lst);
if(!res ){
    cout << "get-share query failed" <<endl;
    STRU_GET_SHARE_RS failure;
    failure.result=net_result_failed;
    SendData(clientfd,(char*)&failure,sizeof(failure));
    return;
}

STRU_GET_SHARE_RS rs;
if( lst.size() == 0 ){
    rs.result = 0;
    SendData( clientfd , (char*)&rs , sizeof(rs));
    return;
}
rs.result = 1;
if( lst.size() %5 != 0 )
{
    rs.result=net_result_failed;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

// Validate every selected top-level item before writing anything. This makes
// the unique index failure visible without leaving a partially saved share.
list<string> checkItems=lst;
while(!checkItems.empty())
{
    checkItems.pop_front();
    string checkName=checkItems.front(); checkItems.pop_front();
    checkItems.pop_front();
    checkItems.pop_front();
    checkItems.pop_front();

    list<string> duplicateResult;
    if(!m_sql->SelectPrepared(
                "select count(*) from t_user_file where u_id=? and f_dir=? and f_name=?;",
                vector<MysqlParam>{MysqlParam::fromInt(rq->userid),
                                   MysqlParam::fromText(rq->dir),
                                   MysqlParam::fromText(checkName)},1,duplicateResult) ||
       duplicateResult.empty())
    {
        rs.result=net_result_failed;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
    if(stoi(duplicateResult.front())>0)
    {
        rs.result=net_result_name_exists;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }
}

if(!m_sql->BeginTransaction())
{
    rs.result=net_result_failed;
    SendData(clientfd,(char*)&rs,sizeof(rs));
    return;
}

bool saveSuccess=true;

//遍历文件列表
while( lst.size() != 0 ){
    int fileid = stoi (lst.front() ); lst.pop_front();
    string name = lst.front(); lst.pop_front();
    string dir = lst.front(); lst.pop_front();
    string type = lst.front(); lst.pop_front();
    int fromuserid = stoi (lst.front() ); lst.pop_front();

    if( type == "file"){
        //如果是文件
        //插入信息到 用户文件关系表
        saveSuccess=GetShareByFile( rq->userid , fileid , rq->dir , name , rq->time );
    }else{
        //如果是文件夹
        //插入信息到 用户文件关系表
        //拼接路径 获取人目录 /->/06/ 分享人的目录 /->/06/
        //根据新路径 在分享人那边查询 文件夹下的文件
        //遍历列表 --> 递归
        saveSuccess=GetShareByFolder( rq->userid , fileid , rq->dir , name , rq->time , fromuserid , dir );
    }
    if(!saveSuccess)break;
}

if(saveSuccess)
{
    saveSuccess=m_sql->CommitTransaction();
}
else
{
    m_sql->RollbackTransaction();
}
rs.result=saveSuccess ? net_result_success : net_result_failed;

//写回复包
strcpy( rs.dir , rq->dir );
//发送
SendData( clientfd , (char*)&rs , sizeof(rs) );
}

bool CLogic::GetShareByFile( int userid, int fileid , string dir , string name , string time )
{
    bool res=m_sql->UpdatePrepared(
                "insert into t_user_file(u_id,f_id,f_dir,f_name,f_uploadtime) "
                "values(?,?,?,?,?);",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromInt(fileid),
                                   MysqlParam::fromText(dir),
                                   MysqlParam::fromText(name),
                                   MysqlParam::fromText(time)});
    if( !res ){
        cout << "save shared file failed" << endl;
    }
    return res;
}

bool CLogic::GetShareByFolder( int userid , int fileid , string dir , string name , string time , int fromuserid , string fromdir )
{
    //是文件夹
    //插入信息到 用户文件关系表
    if(!GetShareByFile( userid , fileid , dir , name , time ))return false;

    //拼接路径
    //获取人目录 /->/06/
    string newdir = dir + name + "/";
    //分享人的目录 /->/06/
    string newfromdir = fromdir + name + "/";

    //根据新路径 在分享人那边查询 文件夹下的文件
    list<string > lst;
    bool res=m_sql->SelectPrepared(
                "select f_id,f_name,f_type from user_file_info where u_id=? and f_dir=?;",
                vector<MysqlParam>{MysqlParam::fromInt(fromuserid),
                                   MysqlParam::fromText(newfromdir)},3,lst);
    if(!res ){
        cout << "shared-folder children query failed" <<endl;
        return false;
    }
    if(lst.size()%3!=0)return false;

    //遍历列表
    while( lst.size() != 0 ){
        int fileid = stoi( lst.front()); lst.pop_front();
        string name = lst.front(); lst.pop_front();
        string type = lst.front(); lst.pop_front();

        if( type == "file"){
            //是文件
            if(!GetShareByFile( userid , fileid , newdir , name , time ))return false;
        }else{
            //是文件夹 ->递归
            if(!GetShareByFolder( userid , fileid , newdir , name , time , fromuserid , newfromdir ))return false;
        }
    }
    return true;
}

bool CLogic::ShareItem(int userid, int fileid, const string &dir,
                       const string &time, int link)
{
    bool res=m_sql->UpdatePrepared(
                "update t_user_file set s_link=?,s_linkTime=? "
                "where u_id=? and f_id=? and f_dir=?;",
                vector<MysqlParam>{MysqlParam::fromInt(link),
                                   MysqlParam::fromText(time),
                                   MysqlParam::fromInt(userid),
                                   MysqlParam::fromInt(fileid),
                                   MysqlParam::fromText(dir)});
    if( !res ){
        cout << "share item update failed" <<endl;
    }
    return res;
}

void CLogic::DeleteFileRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;

    STRU_DELETE_FILE_RS rs;
    if(!szbuf || nlen<static_cast<int>(sizeof(STRU_DELETE_FILE_RQ)))
    {
        rs.result=0;
        SendData(clientfd,reinterpret_cast<char*>(&rs),sizeof(rs));
        return;
    }

    STRU_DELETE_FILE_RQ *rq=
            reinterpret_cast<STRU_DELETE_FILE_RQ*>(szbuf);
    if(!memchr(rq->dir,'\0',sizeof(rq->dir)) || rq->userid<=0 ||
       !IsSafeVirtualDir(rq->dir))
    {
        rs.result=0;
        SendData(clientfd,reinterpret_cast<char*>(&rs),sizeof(rs));
        return;
    }

    strncpy(rs.dir,rq->dir,sizeof(rs.dir)-1);
    const int payload=nlen-sizeof(STRU_DELETE_FILE_RQ);
    const int maxCount=payload/static_cast<int>(sizeof(int));
    if(rq->fileCount<=0 || rq->fileCount>maxCount)
    {
        rs.result=0;
        SendData(clientfd,reinterpret_cast<char*>(&rs),sizeof(rs));
        return;
    }

    bool allSuccess=true;
    const string dir=rq->dir;
    for(int i=0;i<rq->fileCount;++i)
    {
        if(!DeleteItem(rq->userid,rq->fileidArray[i],dir))
        {
            allSuccess=false;
        }
    }

    rs.result=allSuccess?1:0;
    SendData(clientfd,reinterpret_cast<char*>(&rs),sizeof(rs));
}

bool CLogic::DeleteFolderChildren(int userid, const string &dir)
{
    list<string> children;
    if(!m_sql->SelectPrepared(
                "select distinct f_id from user_file_info where u_id=? and f_dir=?;",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromText(dir)},1,children))
    {
        cout<<"select folder children failed"<<endl;
        return false;
    }

    bool allSuccess=true;
    while(!children.empty())
    {
        const int childId=stoi(children.front());
        children.pop_front();
        if(!DeleteItem(userid,childId,dir))
        {
            allSuccess=false;
        }
    }
    return allSuccess;
}

bool CLogic::DeleteItem(int userid, int fileid, const string &dir)
{
    list<string> item;
    if(!m_sql->SelectPrepared(
                "select f_type,f_path,f_name from user_file_info "
                "where u_id=? and f_id=? and f_dir=? limit 1;",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromInt(fileid),
                                   MysqlParam::fromText(dir)},3,item) ||
       item.size()!=3)
    {
        cout<<"delete item not found"<<endl;
        return false;
    }

    const string type=item.front(); item.pop_front();
    const string path=item.front(); item.pop_front();
    const string name=item.front(); item.pop_front();

    if(!m_sql->UpdatePrepared(
                "delete from t_user_file where u_id=? and f_id=? and f_dir=?;",
                vector<MysqlParam>{MysqlParam::fromInt(userid),
                                   MysqlParam::fromInt(fileid),
                                   MysqlParam::fromText(dir)}))
    {
        cout<<"delete user-file relation failed"<<endl;
        return false;
    }

    bool success=true;
    if(type=="folder")
    {
        const string childDir=dir+name+"/";
        success=DeleteFolderChildren(userid,childDir);
    }

    list<string> countResult;
    if(!m_sql->SelectPrepared(
                "select f_count from t_file where f_id=?;",
                vector<MysqlParam>{MysqlParam::fromInt(fileid)},1,countResult))
    {
        cout<<"select reference count failed"<<endl;
        return false;
    }

    // The relation trigger may already have removed the metadata row.
    if(countResult.empty())
    {
        return success;
    }

    const int refCount=stoi(countResult.front());
    if(refCount<=0)
    {
        bool canDeleteMetadata=true;
        if(type=="file")
        {
            // Protect deduplicated files: another metadata record may still
            // reference the same physical path.
            list<string> pathRefs;
            if(!m_sql->SelectPrepared(
                        "select count(*) from t_file where f_path=? "
                        "and f_id<>? and f_count>0;",
                        vector<MysqlParam>{MysqlParam::fromText(path),
                                           MysqlParam::fromInt(fileid)},1,pathRefs) ||
               pathRefs.empty())
            {
                cout<<"select physical path references failed"<<endl;
                return false;
            }

            const int otherRefs=stoi(pathRefs.front());
            if(otherRefs==0 && unlink(path.c_str())!=0 && errno!=ENOENT)
            {
                perror("unlink file fail");
                success=false;
                canDeleteMetadata=false;
            }
        }
        else if(type=="folder")
        {
            // A shared child may still keep this physical directory non-empty.
            if(rmdir(path.c_str())!=0 && errno!=ENOENT && errno!=ENOTEMPTY)
            {
                perror("remove folder fail");
                success=false;
            }
        }

        if(canDeleteMetadata)
        {
            if(!m_sql->UpdatePrepared(
                        "delete from t_file where f_id=? and f_count<=0;",
                        vector<MysqlParam>{MysqlParam::fromInt(fileid)}))
            {
                cout<<"delete file metadata failed"<<endl;
                success=false;
            }
        }
    }

    return success;
}
