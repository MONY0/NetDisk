#ifndef _MYSQL_H
#define _MYSQL_H
#include "packdef.h"
#include <mysql/mysql.h>
#include<list>
#include<string>
#include<vector>

using namespace  std;

struct MysqlParam
{
    enum ValueType { Integer, Text };

    static MysqlParam fromInt(long long value)
    {
        MysqlParam param;
        param.type=Integer;
        param.intValue=value;
        return param;
    }

    static MysqlParam fromText(const string &value)
    {
        MysqlParam param;
        param.type=Text;
        param.textValue=value;
        return param;
    }

    ValueType type;
    long long intValue;
    string textValue;
};

class CMysql{
public:
    CMysql():conn(NULL),m_lockInitialized(false){}
    int ConnectMysql(const char *server, const char *user, const char *password, const char *database);
    int SelectMysql(char* szSql,int nColumn,list<string>& lst);
    int UpdataMysql(char *szsql);
    int SelectPrepared(const char *sql,const vector<MysqlParam> &params,
                       int columnCount,list<string> &rows);
    int UpdatePrepared(const char *sql,const vector<MysqlParam> &params);
    int BeginTransaction();
    int CommitTransaction();
    void RollbackTransaction();
    void DisConnect();
private:
    MYSQL *conn;

    pthread_mutex_t m_lock;
    bool m_lockInitialized;
};




#endif
