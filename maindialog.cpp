#include "maindialog.h"
#include "ui_maindialog.h"
#include<QMessageBox>
#include<QCloseEvent>
#include<QDebug>
#include<QProgressBar>
#include"mytablewidgetitem.h"
#include"packdef.h"
MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainDialog)
{
    ui->setupUi(this);
    //默认文件分页
    ui->sw_page->setCurrentIndex(0);
    //传输默认分页
    ui->tw_transmit->setCurrentIndex(2);
    this->setWindowTitle("网盘");
    this->setWindowFlags(Qt::WindowMinMaxButtonsHint|Qt::WindowCloseButtonHint);
    //添加菜单项
    //先定义菜单项
    QAction *action_addFolder=new QAction(QIcon(":/images/folder.png"),"新建文件夹");
    QAction *action_uploadFie=new QAction("上传文件");
    QAction *action_uploadFolder=new QAction("上传文件夹");
    //添加菜单项
    m_menuAddFile.addAction(action_addFolder);
    m_menuAddFile.addSeparator();//加入分隔符
    m_menuAddFile.addAction(action_uploadFie);
    m_menuAddFile.addAction(action_uploadFolder);

    connect(action_addFolder,SIGNAL(triggered(bool)),this,SLOT(slot_addFolder(bool)));
    connect(action_uploadFie,SIGNAL(triggered(bool)),this,SLOT(slot_uploadFile(bool)));
    connect(action_uploadFolder,SIGNAL(triggered(bool)),this,SLOT(slot_uploadFolder(bool)));
    QAction *action_downloadFile=new QAction("下载文件");

    QAction *action_shareFile=new QAction("分享文件");
    QAction *action_deleteFile=new QAction("删除文件");
    QAction *action_getShare=new QAction("获取分享");
    m_menuFileInfo.addAction(action_addFolder);
    m_menuFileInfo.addSeparator();
    m_menuFileInfo.addAction(action_downloadFile);
    m_menuFileInfo.addAction(action_shareFile);
    m_menuFileInfo.addAction(action_deleteFile);
    m_menuFileInfo.addAction("收藏");
    m_menuFileInfo.addSeparator();
    m_menuFileInfo.addAction(action_getShare);
    connect(action_downloadFile,SIGNAL(triggered(bool)),this,SLOT(slot_downloadFile(bool)));
    connect(action_shareFile,SIGNAL(triggered(bool)),this,SLOT(slot_shareFile(bool)));
    connect(action_deleteFile,SIGNAL(triggered(bool)),this,SLOT(slot_deleteFile(bool)));
    connect(action_getShare,SIGNAL(triggered(bool)),this,SLOT(slot_getShare(bool)));

}

MainDialog::~MainDialog()
{
    delete ui;
}

void MainDialog::closeEvent(QCloseEvent *event)
{
    if(QMessageBox::question(this,"提示","是否退出？")==QMessageBox::Yes)
    {
        //关闭
        event->accept();
        Q_EMIT SIG_close();
    }
    else
    {
        event->ignore();
    }

}


void MainDialog::on_pb_file_clicked()
{
    ui->sw_page->setCurrentIndex(0);
}


void MainDialog::on_pb_transmit_clicked()
{
    ui->sw_page->setCurrentIndex(1);
}


void MainDialog::on_pb_share_clicked()
{
    ui->sw_page->setCurrentIndex(2);
}

//点击添加文件
void MainDialog::on_pb_addFile_clicked()
{
    //弹出菜单
    m_menuAddFile.exec(QCursor::pos());//鼠标的坐标

}
#include<QFileDialog>
//新建文件夹
#include<QInputDialog>
void MainDialog::slot_addFolder(bool flag)
{
    Q_UNUSED(flag);
    qDebug()<<__func__;
    //弹出输入窗口
    QString name=QInputDialog::getText(this,"新建文件夹","输入名称");
    if(name.trimmed().isEmpty() || name.length()>100)
    {
        QMessageBox::about(this,"提示","名字非法");
        return;
    }
    //不可以用的名字....
    //一些非法的 \ / : ? * < >|
    if(name.contains("\\")||name.contains("/")||name.contains(":")||name.contains("?")
            ||name.contains("*")||name.contains("<")||name.contains(">")||name.contains("|")
            ||name.contains("\""))
    {
        QMessageBox::about(this,"提示","名字非法");
        return;
    }
    //判断是否现在已经存在todo

    //发信号 添加文件夹 什么目录下 什么文件
    QString dir=ui->lb_path->text();
    Q_EMIT SIG_addFolder(name,dir);
}
//什么绝对路径的文件 上传到什么目录下
void SIG_uploadFile(QString path,QString dir)
{

}
void MainDialog::slot_uploadFile(bool flag)
{
    qDebug()<<__func__;
    //弹出窗口 选择文件
    QString path=QFileDialog::getOpenFileName(this,"选择文件","./");
    if(path.isEmpty())return;
    //目前上传的有没有一样的东西

    //发送信号 核心类处理 传递信息 上传什么文件到什么目录下
    QString dir=ui->lb_path->text();
    Q_EMIT SIG_uploadFile(path,dir);
}

void MainDialog::slot_uploadFolder(bool flag)
{
    qDebug()<<__func__;
    //点击 弹出文件选择对话框 选择路径
    QString path=QFileDialog::getExistingDirectory(this,"选择文件夹","./");
    //判断为空
    if(path.isEmpty())return;
    //过滤 是否正在上传todo

    //发信号 上传什么路径的文件夹 到什么目录下面
    Q_EMIT SIG_uploadFolder(path,ui->lb_path->text());
}

void MainDialog::slot_downloadFile(bool flag)
{
    qDebug()<<__func__;
    //遍历列表
    int rows=ui->tablefile->rowCount();
      //获取目录
    QString dir=ui->lb_path->text();
    for(int i=0;i<rows;i++)
    {
        // Only download rows explicitly selected by the user.
        MyTableWidgetItem* item0=(MyTableWidgetItem*)ui->tablefile->item(i,0);
        if(!item0 || item0->checkState()!=Qt::Checked)
        {
            continue;
        }
         //获取类型
        if(item0->m_info.type=="file")
        {
             //发信号 下载文件 下载文件夹
            Q_EMIT SIG_downloadFile(item0->m_info.fileid,dir);
        }
        else
        {
            Q_EMIT SIG_downloadFolder(item0->m_info.fileid,dir);
        }

    }


}

void MainDialog::slot_shareFile(bool flag)
{
    qDebug()<<__func__;
    //遍历所有项
    QVector<int>array;
    int count=ui->tablefile->rowCount();
    for(int i=0;i<count;i++)
    {
      MyTableWidgetItem*item0=(MyTableWidgetItem*)ui->tablefile->item(i,0);
      if(!item0)
      {
          continue;
      }
        //看是否是打勾
      if(item0->checkState()==Qt::Checked)
      {
          //添加到数组里面
          array.push_back(item0->m_info.fileid);
          // Do not carry this selection into the next share operation.
          item0->setCheckState(Qt::Unchecked);
      }
    }
    //发送信号
    Q_EMIT SIG_shareFile(array,ui->lb_path->text());
}

void MainDialog::slot_deleteFile(bool flag)
{
    Q_UNUSED(flag);
    qDebug()<<__func__;

    QVector<int> fileids;
    const int rows=ui->tablefile->rowCount();
    for(int i=0;i<rows;++i)
    {
        MyTableWidgetItem *item0=
                static_cast<MyTableWidgetItem*>(ui->tablefile->item(i,0));
        if(item0 && item0->checkState()==Qt::Checked)
        {
            fileids.push_back(item0->m_info.fileid);
        }
    }

    if(fileids.isEmpty())
    {
        QMessageBox::information(this,"提示","请先勾选要删除的文件或文件夹");
        return;
    }

    const QMessageBox::StandardButton answer=QMessageBox::question(
                this,"确认删除",
                QString("确定删除选中的 %1 项吗？此操作不可恢复。")
                    .arg(fileids.size()),
                QMessageBox::Yes|QMessageBox::No,
                QMessageBox::No);
    if(answer!=QMessageBox::Yes)
    {
        return;
    }

    Q_EMIT SIG_deleteFile(fileids,ui->lb_path->text());
}
//#include<QInputDialog>
void MainDialog::slot_getShare(bool flag)
{
    qDebug()<<__func__;
    //弹窗 输入分享码
    QString txt=QInputDialog::getText(this,"获取分享","输入分享码");
    //过滤
    int code=txt.toInt();
    if(txt.length()!=9||code<100000000||code>=1000000000)
    {
        QMessageBox::about(this,"提示","分享码非法");
        return;
    }
    //发送信号 什么目录下面 添加什么分享码的文件
    Q_EMIT SIG_getShareByLink(code,ui->lb_path->text());

}
//插入到上传
void MainDialog::slot_insertUploadFile(FileInfo &info)
{
    //表格插入信息

    //列 文件 大小 时间 速率 进度 按钮
    //1.新增一行,获取当前行+1，设置行数
    int rows=ui->tablefile_upload->rowCount();
    ui->tablefile_upload->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    MyTableWidgetItem*item0=new MyTableWidgetItem;
    item0->slot_setInfo(info);
    ui->tablefile_upload->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(info.size));
    ui->tablefile_upload->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(info.time);
    ui->tablefile_upload->setItem(rows,2,item2);
    QTableWidgetItem*item3=new QTableWidgetItem("0kb/s");
    ui->tablefile_upload->setItem(rows,3,item3);
    //进度条
    QProgressBar *progress=new QProgressBar;
    progress->setMaximum(info.size);
    progress->setValue(info.pos);
    ui->tablefile_upload->setCellWidget(rows,4,progress);
    //按钮
    QPushButton *button=new QPushButton;
    if(info.isPause==0)
    {
        button->setText("暂停");
    }
    else
    {
        button->setText("开始");
    }
    ui->tablefile_upload->setCellWidget(rows,5,button);
    connect(button,&QPushButton::clicked,this,[this,item0,button]()
    {
        bool pause=item0->m_info.isPause==0;
        item0->m_info.isPause=pause ? 1 : 0;
        button->setText(pause ? QString::fromUtf8("继续") : QString::fromUtf8("暂停"));
        Q_EMIT SIG_transferControl(item0->m_info.timestamp,transfer_upload,pause);
    });
}

void MainDialog::slot_insertUploadComplete(FileInfo &info)
{
    //列 文件 大小 时间 上传完成
    int rows=ui->tablefile_complete->rowCount();
    ui->tablefile_complete->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    MyTableWidgetItem*item0=new MyTableWidgetItem;
    item0->slot_setInfo(info);
    ui->tablefile_complete->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(info.size));
    ui->tablefile_complete->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(info.time);
    ui->tablefile_complete->setItem(rows,2,item2);
    QTableWidgetItem*item3=new QTableWidgetItem("上传完成");
    ui->tablefile_complete->setItem(rows,3,item3);
}

void MainDialog::slot_insertDownloadComplete(FileInfo &info)
{

    int rows=ui->tablefile_complete->rowCount();
    ui->tablefile_complete->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    MyTableWidgetItem*item0=new MyTableWidgetItem;
    item0->slot_setInfo(info);
    ui->tablefile_complete->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(info.size));
    ui->tablefile_complete->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(info.time);
    ui->tablefile_complete->setItem(rows,2,item2);
    QPushButton *button=new QPushButton;

    button->setIcon(QIcon(":/images/folder.png"));
    button->setFlat(true);
    ///tolotip提示
    button->setToolTip(info.absolutePath);
    connect(button,SIGNAL(clicked(bool)),this,SLOT(slot_openPath(bool)));

    ui->tablefile_complete->setCellWidget(rows,3,button);
}

void MainDialog::slot_insertShareFileInfo(QString name, int size, QString time, int sharelink)
{
    //列 文件 大小 时间 分享码
    int rows=ui->tablefile_share->rowCount();
    ui->tablefile_share->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    QTableWidgetItem*item0=new QTableWidgetItem(name);

    ui->tablefile_share->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(size));

    ui->tablefile_share->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(time);
    ui->tablefile_share->setItem(rows,2,item2);

    QTableWidgetItem*item3=new QTableWidgetItem(QString::number(sharelink));
    ui->tablefile_share->setItem(rows,3,item3);
}
#include<QProcess>
void MainDialog::slot_openPath(bool flag)
{
    Q_UNUSED(flag);

    QPushButton *button=qobject_cast<QPushButton*>(QObject::sender());
    if(!button)
    {
        return;
    }

    QString path=QDir::toNativeSeparators(button->toolTip());
    if(path.isEmpty())
    {
        return;
    }

    qDebug()<<path;
    //如何打开文件夹

    //通过Qt打开进程
    QStringList lst;
    lst<<QString("/select,")<<path;
    QProcess::startDetached("explorer.exe",lst);

}
void MainDialog::slot_updateUploadFileProgree(int timestamp, int pos)
{
    //遍历所有的项 第0列
    int row=ui->tablefile_upload->rowCount();
    //取导每一个文件信息的时间戳 看是否一致
    for(int i=0;i<row;i++)
    {
      MyTableWidgetItem*item0=(MyTableWidgetItem*)ui->tablefile_upload->item(i,0);
      if(item0->m_info.timestamp==timestamp)
      {
           //一致，更新进度
          QProgressBar *item4=(QProgressBar*)ui->tablefile_upload->cellWidget(i,4);
          item0->m_info.pos=pos;
          item4->setValue(pos);
           //看是否结束
          if(item4->value()>=item4->maximum())
          {

              //是 删除这一项 添加到完成
              slot_insertUploadComplete(item0->m_info);
              slot_deleteUploadFileByRow(i);
              return;
          }
      }
    }

}

void MainDialog::slot_updateDownloadFileProgree(int timestamp, int pos)
{
    //遍历所有的项 第0列
    int row=ui->tablefile_download->rowCount();
    //取导每一个文件信息的时间戳 看是否一致
    for(int i=0;i<row;i++)
    {
      MyTableWidgetItem*item0=(MyTableWidgetItem*)ui->tablefile_download->item(i,0);
      if(!item0)
      {
          continue;
      }
      if(item0->m_info.timestamp==timestamp)
      {
           //一致，更新进度
          QProgressBar *item4=qobject_cast<QProgressBar*>(ui->tablefile_download->cellWidget(i,4));
          if(!item4)
          {
              return;
          }
          item0->m_info.pos=pos;
          item4->setValue(pos);
           //看是否结束
          if(item4->value()>=item4->maximum())
          {
              // Keep the completed task in the download list and also add it
              // to the completed list.
              slot_insertDownloadComplete(item0->m_info);

              QPushButton *button=qobject_cast<QPushButton*>(
                          ui->tablefile_download->cellWidget(i,5));
              if(button)
              {
                  button->setText("已完成");
                  button->setEnabled(false);
              }
              return;
          }
      }
    }
}

void MainDialog::slot_deleteUploadFileByRow(int row)
{

}

void MainDialog::slot_deleteDownloadFileByRow(int row)
{
    if(row>=0 && row<ui->tablefile_download->rowCount())
    {
        ui->tablefile_download->removeRow(row);
    }
}



// Clear stale rows before requesting the current directory again.
void MainDialog::slot_clearFileList()
{
    ui->tablefile->clearContents();
    ui->tablefile->setRowCount(0);
}

void MainDialog::slot_insertFileInfo(FileInfo &info)
{
    //列 文件 大小 时间 上传完成
    int rows=ui->tablefile->rowCount();
    ui->tablefile->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    MyTableWidgetItem*item0=new MyTableWidgetItem;
    item0->slot_setInfo(info);
    ui->tablefile->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(info.size));
    ui->tablefile->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(info.time);
     ui->tablefile->setItem(rows,2,item2);
}

void MainDialog::slot_insertDownloadFile(FileInfo &info)
{
    //列 文件 大小 时间 速率 进度 按钮
    //1.新增一行,获取当前行+1，设置行数
    int rows=ui->tablefile_download->rowCount();
    ui->tablefile_download->setRowCount(rows+1);
    //2.设置这一行的每一列控件
    MyTableWidgetItem*item0=new MyTableWidgetItem;
    item0->slot_setInfo(info);
    ui->tablefile_download->setItem(rows,0,item0);
    QTableWidgetItem*item1=new QTableWidgetItem(FileInfo::getSize(info.size));
    ui->tablefile_download->setItem(rows,1,item1);
     QTableWidgetItem*item2=new QTableWidgetItem(info.time);
    ui->tablefile_download->setItem(rows,2,item2);
    QTableWidgetItem*item3=new QTableWidgetItem("0kb/s");
    ui->tablefile_download->setItem(rows,3,item3);
    //进度条
    QProgressBar *progress=new QProgressBar;
    progress->setMaximum(info.size);
    progress->setValue(info.pos);
    ui->tablefile_download->setCellWidget(rows,4,progress);
    //按钮
    QPushButton *button=new QPushButton;
    if(info.isPause==0)
    {
        button->setText("暂停");
    }
    else
    {
        button->setText("开始");
    }
    ui->tablefile_download->setCellWidget(rows,5,button);
    connect(button,&QPushButton::clicked,this,[this,item0,button]()
    {
        bool pause=item0->m_info.isPause==0;
        item0->m_info.isPause=pause ? 1 : 0;
        button->setText(pause ? QString::fromUtf8("继续") : QString::fromUtf8("暂停"));
        Q_EMIT SIG_transferControl(item0->m_info.timestamp,transfer_download,pause);
    });
}

void MainDialog::slot_deleteAllFileInfo()
{
    //ui->tablefile->clear();//删文字 不用这个 行数不变
    int rows=ui->tablefile->rowCount();
    for(int i=rows-1;i>=0;i--)
    {
        ui->tablefile->removeRow(i);
    }
}

void MainDialog::slot_deleteALLShareInfo()
{
    ui->tablefile_share->clearContents();
    ui->tablefile_share->setRowCount(0);
}

//选中某一行
void MainDialog::on_tablefile_cellClicked(int row, int column)
{
    //切换 勾选未勾选状态
    MyTableWidgetItem *item0=(MyTableWidgetItem*)ui->tablefile->item(row,0);
    if(item0->checkState()==Qt::Checked)
    {
        item0->setCheckState(Qt::Unchecked);
    }
    else
    {
        item0->setCheckState(Qt::Checked);
    }
}

//表格位置鼠标右键
void MainDialog::on_tablefile_customContextMenuRequested(const QPoint &pos)
{

    m_menuFileInfo.exec(QCursor::pos());
}


void MainDialog::on_tablefile_cellDoubleClicked(int row, int column)
{
    //首先 拿到双击的那行的文件名字
    MyTableWidgetItem *item0=(MyTableWidgetItem*)ui->tablefile->item(row,0);
    //判断 是不是文件夹 （不是的话打开）
    if(item0->m_info.type!="file")
    {
        //路径 拼接
        QString dir=ui->lb_path->text()+item0->m_info.name+"/";
        //设置路径
        ui->lb_path->setText(dir);
        //发送信号-》更新当前的目录-》刷新文件列表
        Q_EMIT SIG_changeDir(dir);
    }
}


void MainDialog::on_pb_prev_clicked()
{
    //获取目录
    QString path=ui->lb_path->text();
    //判断"/" 结束
    if(path=="/")return;
    //首先找到最右边的"/"然后从它左边，开始再向又找"/"
    path=path.left(path.lastIndexOf("/"));
    path=path.left(path.lastIndexOf("/")+1);
    //新的目录就是 找到的"/" 以及左边的所有字符
    qDebug()<<path;
    ui->lb_path->setText(path);
    //跳转路径
    Q_EMIT SIG_changeDir(path);
}

