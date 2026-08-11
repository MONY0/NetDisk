#ifndef CSQLITE_H
#define CSQLITE_H
#include <QStringList>
#include <QSqlDatabase>
#include <QMutex>

class CSqlite
{
public:
    CSqlite();
    ~CSqlite();
    bool ConnectSql(const QString &dbPath);
    void DisConnect();
    bool SelectSql(const QString &sqlStr,int columnCount,QStringList &values);
    bool UpdateSql(const QString &sqlStr);
private:
    QSqlDatabase m_db;
    QMutex m_mutex;
};
#endif
