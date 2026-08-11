#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <QDialog>
#include<QCloseEvent>
#include<QMenu>
#include"common.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainDialog; }
QT_END_NAMESPACE
class CKelnel;
class MainDialog : public QDialog
{
    Q_OBJECT
signals:
    void SIG_close();
    void SIG_uploadFile(QString path,QString dir);
    //什么文件id的文件，什么目录下的文件 下载
    void SIG_downloadFile(int fileid,QString dir);
    //什么文件id的文件，什么目录下的文件夹 下载
    void SIG_downloadFolder(int fileif,QString dir);
    void SIG_addFolder(QString name,QString dir);
    void SIG_uploadFolder(QString path,QString dir);
    //改变路径
    void SIG_changeDir(QString dir);
    //分享
    void SIG_shareFile(QVector<int>fileidArray,QString dir);

    void SIG_getShareByLink(int code,QString dir);
    void SIG_deleteFile(QVector<int> fileidArray,QString dir);
    void SIG_transferControl(int timestamp,int direction,bool pause);
public:
    MainDialog(QWidget *parent = nullptr);
    ~MainDialog();


    void closeEvent(QCloseEvent*event);//重写虚函数
private slots:
    void on_pb_file_clicked();

    void on_pb_transmit_clicked();

    void on_pb_share_clicked();

    void on_pb_addFile_clicked();

    void slot_addFolder(bool flag);
    void slot_uploadFile(bool flag);
    void slot_uploadFolder(bool flag);
    void slot_downloadFile(bool flag);
    void slot_shareFile(bool flag);
    void slot_deleteFile(bool flag);
    void slot_getShare(bool flag);
    void slot_insertUploadFile(FileInfo &info);
    void slot_insertUploadComplete(FileInfo &info);
    void slot_insertDownloadComplete(FileInfo &info);
    void slot_insertShareFileInfo(QString name,int size,QString time,int sharelink);
    void slot_updateUploadFileProgree(int timestamp,int pos);
    void slot_updateDownloadFileProgree(int timestamp,int pos);
    void slot_deleteUploadFileByRow(int row);

    void slot_deleteDownloadFileByRow(int row);
    void slot_openPath(bool flag);
    void slot_clearFileList();
    void slot_insertFileInfo(FileInfo &info);
    void slot_insertDownloadFile(FileInfo &info);
    void slot_deleteAllFileInfo();
    void slot_deleteALLShareInfo();
    void on_tablefile_cellClicked(int row, int column);

    void on_tablefile_customContextMenuRequested(const QPoint &pos);

    void on_tablefile_cellDoubleClicked(int row, int column);

    void on_pb_prev_clicked();

private:
    Ui::MainDialog *ui;
    QMenu m_menuAddFile;
    QMenu m_menuFileInfo;

    friend class CKernel;
};
#endif // MAINDIALOG_H
