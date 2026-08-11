#include "clogic.h"

void CLogic::setNetPackMap()
{
    NetPackMap(_DEF_PACK_REGISTER_RQ)    = &CLogic::RegisterRq;
    NetPackMap(_DEF_PACK_LOGIN_RQ)       = &CLogic::LoginRq;
    NetPackMap(_DEF_PACK_UPLOAD_FILE_RQ) = &CLogic::UploadFileRq;
    NetPackMap(_DEF_PACK_FILE_CONTENT_RQ)=&CLogic::FileContentRq;
}

#define _DEF_COUT_FUNC_    cout << "clientfd:"<< clientfd << __func__ << endl;

#define DEF_PATH "/home/coder/NetDisk/"
//注册
#define DEF_PATH "/home/coder/NetDisk/"
//注册
void CLogic::RegisterRq(sock_fd clientfd,char* szbuf,int nlen)
{
    _DEF_COUT_FUNC_
    STRU_REGISTER_RQ* rq = (STRU_REGISTER_RQ*)szbuf;
    STRU_REGISTER_RS rs;
    //memset(&rs, 0, sizeof(rs)); // 初始化返回包

    char sqlstr[1024] = "";
    list<string> lstRes;

    // 1.校验手机号是否已存在
    sprintf(sqlstr,"select u_tel from t_user where u_tel='%s';",rq->tel);
    bool queryRet = m_sql->SelectMysql(sqlstr, 1, lstRes);
    if(!queryRet)
    {
        std::cout<<"手机号查询失败:"<<sqlstr<<std::endl;
    }
    if(lstRes.size() != 0)
    {
        rs.result = tel_is_exist;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return; // 手机号已存在直接返回，不再往下执行
    }

    // 2.校验昵称是否重复
    lstRes.clear();
    sprintf(sqlstr,"select u_tel from t_user where u_name='%s';",rq->name);
    queryRet = m_sql->SelectMysql(sqlstr, 1, lstRes);
    if(!queryRet)
    {
        std::cout<<"昵称查询失败:"<<sqlstr<<std::endl;
    }
    if(lstRes.size() != 0)
    {
        rs.result = name_is_exist;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    // 3.手机号、昵称都不重复，执行插入用户
    rs.result = register_success;
    sprintf(sqlstr,"insert into t_user(u_tel,u_password,u_name) values ('%s','%s','%s');",
            rq->tel, rq->password, rq->name);
    bool updateRet = m_sql->UpdataMysql(sqlstr);
    if(!updateRet)
    {
        std::cout<<"用户插入失败:"<<sqlstr<<std::endl;
        SendData(clientfd,(char*)&rs,sizeof(rs));
        return;
    }

    // 4.查询新注册用户u_id
    lstRes.clear();
    sprintf(sqlstr,"select u_id from t_user where u_tel='%s' and u_password='%s';",
            rq->tel, rq->password);
    queryRet = m_sql->SelectMysql(sqlstr, 1, lstRes);
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
    SendData(clientfd,(char*)&rs,sizeof(rs));
}



//登录
void CLogic::LoginRq(sock_fd clientfd ,char* szbuf,int nlen)
{
    _DEF_COUT_FUNC_
    // 把接收的数据强转为登录请求结构体
    STRU_LOGIN_RQ*rq=(STRU_LOGIN_RQ*)szbuf;
    // 登录返回包
    STRU_LOGIN_RS rs;
    char sqlstr[1024] = "";
    list<string> lstRes;

    // 根据手机号查询用户ID、密文密码、昵称三列数据
    sprintf(sqlstr,"select u_id,u_password,u_name from t_user where u_tel='%s';",rq->tel);
    bool queryRet = m_sql->SelectMysql(sqlstr, 3, lstRes);
    if(!queryRet)
    {
        std::cout<<"查询失败:"<<sqlstr<<std::endl;
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


    //m_mapTimestampToFileInfo
    //1 拆包
    STRU_UPLOAD_FILE_RQ *rq=(STRU_UPLOAD_FILE_RQ*)szbuf;
    //2 查看是否秒传 todo
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
    //4 文件信息创建  打开文件
     info->filefd=open(strpath,O_CREAT|O_WRONLY|O_TRUNC,00777);
     if(info->filefd<0)
     {
         std::cout<<"file open fialed"<<std::endl;
         return;
     }
    //5 map 存储文件信息
     int64_t user_time=rq->userid*getNumber()+rq->timestamp;
     m_mapTimestampToFileInfo.insert(user_time,info);
    //6 数据库记录
    //7 插入文件信息（引用计数 0 ，状态 0 -> 上传结束后改为 1 ）
     char sqlbuf[1000]="";
     sprintf(sqlbuf, "insert into t_file (f_size, f_path, f_md5, f_count, f_state, f_type) values (%d, '%s', '%s', 0, 0, 'file');", rq->size, strpath, rq->md5);
     bool res=m_sql->UpdataMysql(sqlbuf);
     if(!res)
     {
         printf("update fail:%s\n",sqlbuf);
     }
    //8 查文件 id
     sprintf(sqlbuf, "select f_id from t_file where f_path = '%s' and f_MD5 = '%s'", strpath, rq->md5);
     list<string> lstRes;
      res = m_sql->SelectMysql(sqlbuf, 1, lstRes);
     if( !res ){
         printf( "Select fail:%s\n" , sqlbuf );
     }
     if(lstRes.size()>0)
     {
         info->fid=stoi(lstRes.front());
     }
     lstRes.clear();
    //9 插入用户文件关系（由于触发器 引用计数 ->1 ）
     sprintf(sqlbuf, "insert into t_user_file ( u_id , f_id , f_dir , f_name , f_uploadtime ) values( %d , %d , '%s' , '%s' , '%s' );", rq->userid , info->fid, rq->dir , rq->fileName , rq->time );

     res = m_sql->UpdataMysql( sqlbuf );
     if( !res ){
         printf( "update fail:%s\n" , sqlbuf );
     }

    //10 写回复包
     STRU_UPLOAD_FILE_RS rs;
     rs.fileid=info->fid;
     rs.result=1;
     rs.timestamp=rq->timestamp;
     rs.userid=rq->userid;
     SendData(clientfd,(char*)&rs,sizeof(rs));
}

void CLogic::FileContentRq(sock_fd clientfd, char *szbuf, int nlen)
{
    _DEF_COUT_FUNC_;
    STRU_FILE_CONTENT_RQ*rq=(STRU_FILE_CONTENT_RQ*)szbuf;
    int64_t user_time=rq->userid *getNumber()+rq->timestamp;
    FileInfo *info=nullptr;
    if(!m_mapTimestampToFileInfo.find(user_time,info))
    {
        cout<<"file not found"<<endl;
        return;
    }
    STRU_FILE_CONTENT_RS rs;
    int len=write(info->filefd,rq->content,rq->len);
    if(len!=rq->len)
    {
        rs.result=0;
        lseek(info->filefd,-1*len,SEEK_CUR);
    }
    else
    {
        rs.result=1;
        info->pos+=len;
        if(info->pos>=info->size)
        {
            close(info->filefd);
            m_mapTimestampToFileInfo.erase(user_time);
            delete info;
            info =nullptr;
            //geng xin shujuku
            char sqlbuf[1000]="";
            sprintf(sqlbuf,"update t_file set f_state=1 where f_id=%d;",rq->fileid);
            bool res=m_sql->UpdataMysql(sqlbuf);
            if(!res)
            {
                cout<<"update fail :"<<sqlbuf<<endl;
            }
        }
    }
    rs.fileid=rq->fileid;
    rs.len=rq->len;
    rs.timestamp=rq->timestamp;
    rs.userid=rq->userid;

    SendData(clientfd,(char*)&rs,sizeof(rs));
}
