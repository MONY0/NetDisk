#include "csqlite.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QMutexLocker>
#include <QDebug>

CSqlite::CSqlite()
{
    const QString name="netdisk_transfer_cache";
    m_db=QSqlDatabase::contains(name) ? QSqlDatabase::database(name)
                                      : QSqlDatabase::addDatabase("QSQLITE",name);
}
CSqlite::~CSqlite(){ DisConnect(); }

bool CSqlite::ConnectSql(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_db.setDatabaseName(path);
    if(!m_db.open())
    {
        qDebug()<<"open sqlite failed"<<path<<m_db.lastError().text();
        return false;
    }
    return true;
}
void CSqlite::DisConnect()
{
    QMutexLocker locker(&m_mutex);
    if(m_db.isOpen())m_db.close();
}
bool CSqlite::SelectSql(const QString &sql,int columns,QStringList &values)
{
    QMutexLocker locker(&m_mutex);
    if(!m_db.isOpen()&&!m_db.open())return false;
    QSqlQuery query(m_db);
    if(!query.exec(sql))
    {
        qDebug()<<"sqlite select failed"<<query.lastError().text()<<sql;
        return false;
    }
    values.clear();
    while(query.next())for(int i=0;i<columns;++i)values<<query.value(i).toString();
    return true;
}
bool CSqlite::UpdateSql(const QString &sql)
{
    QMutexLocker locker(&m_mutex);
    if(!m_db.isOpen()&&!m_db.open())return false;
    QSqlQuery query(m_db);
    if(!query.exec(sql))
    {
        qDebug()<<"sqlite update failed"<<query.lastError().text()<<sql;
        return false;
    }
    return true;
}
