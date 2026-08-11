#include "Mysql.h"
#include <cstring>
#include <iostream>

namespace
{
class MutexGuard
{
public:
    explicit MutexGuard(pthread_mutex_t *lock):m_lock(lock)
    {
        pthread_mutex_lock(m_lock);
    }
    ~MutexGuard()
    {
        pthread_mutex_unlock(m_lock);
    }
private:
    pthread_mutex_t *m_lock;
};

bool bindPreparedParams(MYSQL_STMT *stmt,const vector<MysqlParam> &params,
                        vector<MYSQL_BIND> &binds,
                        vector<long long> &intValues,
                        vector<unsigned long> &textLengths)
{
    if(params.empty())return true;

    binds.resize(params.size());
    intValues.resize(params.size());
    textLengths.resize(params.size());
    memset(binds.data(),0,binds.size()*sizeof(MYSQL_BIND));

    for(size_t i=0;i<params.size();++i)
    {
        if(params[i].type==MysqlParam::Integer)
        {
            intValues[i]=params[i].intValue;
            binds[i].buffer_type=MYSQL_TYPE_LONGLONG;
            binds[i].buffer=&intValues[i];
            binds[i].is_unsigned=0;
        }
        else
        {
            textLengths[i]=static_cast<unsigned long>(params[i].textValue.size());
            binds[i].buffer_type=MYSQL_TYPE_STRING;
            binds[i].buffer=const_cast<char*>(params[i].textValue.data());
            binds[i].buffer_length=textLengths[i];
            binds[i].length=&textLengths[i];
        }
    }
    return mysql_stmt_bind_param(stmt,binds.data())==0;
}
}



int CMysql::ConnectMysql(const char* server,const char* user,const char* password, const char* database)
{
    conn=mysql_init(NULL);
    if(!conn)return FALSE;
    if(!mysql_real_connect(conn,server,user,password,database,0,NULL,0))
    {
        mysql_close(conn);
        conn=NULL;
        return FALSE;
    }
    if(mysql_set_character_set(conn,"utf8mb4")!=0)
    {
        mysql_close(conn);
        conn=NULL;
        return FALSE;
    }

    pthread_mutexattr_t attr;
    if(pthread_mutexattr_init(&attr)!=0)return FALSE;
    if(pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE)!=0 ||
       pthread_mutex_init(&m_lock,&attr)!=0)
    {
        pthread_mutexattr_destroy(&attr);
        mysql_close(conn);
        conn=NULL;
        return FALSE;
    }
    pthread_mutexattr_destroy(&attr);
    m_lockInitialized=true;
    return TRUE;
#if 0
    conn = mysql_init(NULL);  // 创建MYSQL对象。NULL表示加载默认值。
    if(!conn)return FALSE;
    if(!mysql_real_connect(conn,server,user,password,database,0,NULL,0))//连接数据库
    {
        return FALSE;// 连接失败，返回FALSE
    }
    mysql_set_character_set(conn,"utf8mb4");
    // A transaction holds this lock across several SQL calls. Recursive mode
    // lets those calls reuse the same lock while all other threads stay out.
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_lock,&attr);
    pthread_mutexattr_destroy(&attr);

    return TRUE;  // 连接成功，返回TRUE
#endif
}

int CMysql::SelectMysql(char* szSql,int nColumn,list<string>& lst)
{
    if(!conn || !m_lockInitialized || !szSql || nColumn<=0)return FALSE;
    MutexGuard guard(&m_lock);
    if(mysql_query(conn,szSql))return FALSE;
    MYSQL_RES *results=mysql_store_result(conn);
    if(!results)return FALSE;
    if(mysql_num_fields(results)<static_cast<unsigned int>(nColumn))
    {
        mysql_free_result(results);
        return FALSE;
    }
    MYSQL_ROW record;
    while((record=mysql_fetch_row(results))!=NULL)
    {
        for(int i=0;i<nColumn;++i)
            lst.push_back(record[i] ? record[i] : "");
    }
    mysql_free_result(results);
    return TRUE;
#if 0
    MYSQL_RES * results = NULL;

    pthread_mutex_lock(&m_lock ); // 加锁

    if(mysql_query(conn,szSql)) {  // 查询SQL是否合法
        pthread_mutex_unlock(&m_lock );  // 查询失败，解锁并返回FALSE
        return FALSE;  
    }
    results = mysql_store_result(conn);  // 将结果放在result中
    pthread_mutex_unlock(&m_lock );   // 解锁

    if(NULL == results)return FALSE;   // result为空，返回FALSE
    MYSQL_ROW record;     // 定义记录行
    while((record = mysql_fetch_row(results)))  // 遍历查询结果
    {
        for(int i=0; i<nColumn; i++)   
        {
            lst.push_back(record[i] ? record[i] : "");
        }
    }
    mysql_free_result(results);
    return TRUE;  // 查询成功，返回TRUE
#endif
}


int CMysql::UpdataMysql(char *szsql)
{
    if(!szsql)return FALSE;   // SQL为空，返回FALSE
    pthread_mutex_lock(&m_lock );   // 加锁
    if(mysql_query(conn,szsql)){    // 执行SQL语句
        pthread_mutex_unlock(&m_lock );  // 执行失败，解锁并返回FALSE
        return FALSE;
    }
    pthread_mutex_unlock(&m_lock );  // 解锁
    return TRUE;   // SQL执行成功，返回TRUE
}

int CMysql::UpdatePrepared(const char *sql,const vector<MysqlParam> &params)
{
    if(!conn || !m_lockInitialized || !sql)return FALSE;
    MutexGuard guard(&m_lock);
    MYSQL_STMT *stmt=mysql_stmt_init(conn);
    if(!stmt)return FALSE;

    bool success=mysql_stmt_prepare(stmt,sql,static_cast<unsigned long>(strlen(sql)))==0;
    if(success)success=mysql_stmt_param_count(stmt)==params.size();
    vector<MYSQL_BIND> binds;
    vector<long long> intValues;
    vector<unsigned long> textLengths;
    if(success)
        success=bindPreparedParams(stmt,params,binds,intValues,textLengths);
    if(success)
        success=mysql_stmt_execute(stmt)==0;
    if(!success)
        cerr<<"prepared update failed: "<<mysql_stmt_error(stmt)<<endl;

    mysql_stmt_close(stmt);
    return success ? TRUE : FALSE;
}

int CMysql::SelectPrepared(const char *sql,const vector<MysqlParam> &params,
                           int columnCount,list<string> &rows)
{
    if(!conn || !m_lockInitialized || !sql || columnCount<=0)return FALSE;
    MutexGuard guard(&m_lock);
    MYSQL_STMT *stmt=mysql_stmt_init(conn);
    if(!stmt)return FALSE;

    bool success=mysql_stmt_prepare(stmt,sql,static_cast<unsigned long>(strlen(sql)))==0;
    if(success)success=mysql_stmt_param_count(stmt)==params.size();
    vector<MYSQL_BIND> paramBinds;
    vector<long long> intValues;
    vector<unsigned long> textLengths;
    if(success)
        success=bindPreparedParams(stmt,params,paramBinds,intValues,textLengths);

    my_bool updateMaxLength=1;
    if(success)
        mysql_stmt_attr_set(stmt,STMT_ATTR_UPDATE_MAX_LENGTH,&updateMaxLength);
    if(success)
        success=mysql_stmt_execute(stmt)==0;

    MYSQL_RES *metadata=success ? mysql_stmt_result_metadata(stmt) : nullptr;
    if(success && (!metadata || mysql_num_fields(metadata)!=static_cast<unsigned int>(columnCount)))
        success=false;
    if(success)
        success=mysql_stmt_store_result(stmt)==0;

    vector<MYSQL_BIND> resultBinds;
    vector<vector<char> > buffers;
    vector<unsigned long> resultLengths;
    vector<my_bool> isNull;
    if(success)
    {
        MYSQL_FIELD *fields=mysql_fetch_fields(metadata);
        resultBinds.resize(columnCount);
        buffers.resize(columnCount);
        resultLengths.resize(columnCount);
        isNull.resize(columnCount);
        memset(resultBinds.data(),0,resultBinds.size()*sizeof(MYSQL_BIND));
        for(int i=0;i<columnCount;++i)
        {
            const unsigned long capacity=fields[i].max_length+1;
            buffers[i].resize(capacity>1 ? capacity : 1);
            resultBinds[i].buffer_type=MYSQL_TYPE_STRING;
            resultBinds[i].buffer=buffers[i].data();
            resultBinds[i].buffer_length=static_cast<unsigned long>(buffers[i].size());
            resultBinds[i].length=&resultLengths[i];
            resultBinds[i].is_null=&isNull[i];
        }
        success=mysql_stmt_bind_result(stmt,resultBinds.data())==0;
    }

    while(success)
    {
        const int fetchResult=mysql_stmt_fetch(stmt);
        if(fetchResult==MYSQL_NO_DATA)break;
        if(fetchResult!=0)
        {
            success=false;
            break;
        }
        for(int i=0;i<columnCount;++i)
        {
            if(isNull[i])
            {
                rows.push_back("");
                continue;
            }
            const unsigned long safeLength=
                    resultLengths[i]<buffers[i].size() ? resultLengths[i]
                                                       : static_cast<unsigned long>(buffers[i].size()-1);
            rows.push_back(string(buffers[i].data(),safeLength));
        }
    }

    if(!success)
        cerr<<"prepared select failed: "<<mysql_stmt_error(stmt)<<endl;
    if(metadata)mysql_free_result(metadata);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return success ? TRUE : FALSE;
}

int CMysql::BeginTransaction()
{
    if(!conn || !m_lockInitialized)return FALSE;
    pthread_mutex_lock(&m_lock);
    if(mysql_query(conn,"START TRANSACTION;"))
    {
        pthread_mutex_unlock(&m_lock);
        return FALSE;
    }
    // Keep the outer recursive lock until commit or rollback.
    return TRUE;
}

int CMysql::CommitTransaction()
{
    if(!conn || !m_lockInitialized)return FALSE;
    int result=TRUE;
    if(mysql_query(conn,"COMMIT;"))
    {
        mysql_query(conn,"ROLLBACK;");
        result=FALSE;
    }
    pthread_mutex_unlock(&m_lock);
    return result;
}

void CMysql::RollbackTransaction()
{
    if(!conn || !m_lockInitialized)return;
    mysql_query(conn,"ROLLBACK;");
    pthread_mutex_unlock(&m_lock);
}

void CMysql::DisConnect()
{
    if(conn)
    {
        mysql_close(conn);
        conn=NULL;
    }
    if(m_lockInitialized)
    {
        pthread_mutex_destroy(&m_lock);
        m_lockInitialized=false;
    }
#if 0
    mysql_close(conn);  // 关闭数据库连接
    pthread_mutex_destroy(&m_lock);
#endif
}
