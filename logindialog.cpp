#include "logindialog.h"
#include "ui_logindialog.h"
#include<QMessageBox>
LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle("登录&注册");
    //窗口默认注册
    ui->tb_login->setCurrentIndex(1);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}
//注册按钮
void LoginDialog::on_pb_regiser_clicked()
{
    //注册采集信息
    QString tel=ui->le_tel_2->text();
    QString password=ui->le_passward_2->text();
    QString confirm=ui->le_passward_3->text();
    QString name=ui->le_name->text();

    QString tmpName=name;
    //过滤
    //查看是否输入为空
    if(tel.isEmpty()||password.isEmpty()||confirm.isEmpty()||name.isEmpty()||tmpName.remove(" ").isEmpty())
    {
        QMessageBox::about(this,"提示","输入内容，不能为空");
        return;
    }
    //手机号是否合法
    QRegExp exp("^1[356789][0-9]\{9\}$");
    bool res=exp.exactMatch(tel);
    if(!res)
    {
        QMessageBox::about(this,"提示","手机号非法");

    }
    //密码是否过程
    if(password.size()>20)
    {
        QMessageBox::about(this,"提示","密码过长");

    }
    //两次不一致
    if(confirm!=password)
    {
        QMessageBox::about(this,"提示","两次密码不同");
    }
    //昵称 是否过长---敏感词汇的过滤
    if(name.size()>10)
    {
        QMessageBox::about(this,"提示","名字过长");
    }
    //发信号
    Q_EMIT SIG_RegisterCommit(tel,password,name);
}


void LoginDialog::on_pb_clear_2_clicked()
{
     ui->le_tel_2->setText("");
     ui->le_name->setText("");
     ui->le_passward_2->setText("");
     ui->le_passward_3->setText("");
}


void LoginDialog::on_pb_login_clicked()
{
    //注册采集信息
    QString tel=ui->le_tel->text();
    QString password=ui->le_passward->text();
    //过滤
    //查看是否输入为空
    if(tel.isEmpty()||password.isEmpty())
    {
        QMessageBox::about(this,"提示","输入内容，不能为空");
        return;
    }
    //手机号是否合法
    QRegExp exp("^1[356789][0-9]\{9\}$");
    bool res=exp.exactMatch(tel);
    if(!res)
    {
        QMessageBox::about(this,"提示","手机号非法");
        return;
    }
    //密码是否过程
    if(password.size()>20)
    {
        QMessageBox::about(this,"提示","密码过长");
        return;
    }

    //发信号
    Q_EMIT SIG_LoginCommit(tel,password);

}


void LoginDialog::on_pb_clear_clicked()
{
    ui->le_tel->setText("");
    ui->le_passward->setText("");
}

