#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
signals:
    void SIG_RegisterCommit(QString tel,QString passward,QString name);
    void SIG_LoginCommit(QString tel,QString passward);
private slots:
    void on_pb_regiser_clicked();

    void on_pb_clear_2_clicked();

    void on_pb_login_clicked();

    void on_pb_clear_clicked();

private:
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
