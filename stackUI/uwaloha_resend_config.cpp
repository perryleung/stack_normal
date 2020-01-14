#include "uwaloha_resend_config.h"
#include "ui_uwaloha_resend_config.h"
#include <QValidator>
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>


extern QString python_path;
uwaloha_resend_config::uwaloha_resend_config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::uwaloha_resend_config)
{

    ui->setupUi(this);
    init_();

    connect(ui->btn_ualoha_config, SIGNAL(clicked(bool)), this, SLOT(set_uwaloha_resend_config()));
}

void uwaloha_resend_config::init_(){
    ui->le_uwaloha_min_time->setValidator(new QIntValidator(0, 20, this));
    ui->le_uwaloha_max_time->setValidator(new QIntValidator(20, 100, this));
    setWindowTitle("Resend Time Setting");
    ui->le_uwaloha_min_time->setText("15");
    ui->le_uwaloha_max_time->setText("25");
}
void uwaloha_resend_config::set_uwaloha_resend_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    int min_time = ui->le_uwaloha_min_time->text().toInt();
    int max_time = ui->le_uwaloha_max_time->text().toInt();
    QString script("./python_script/set_aloha_send.py ");
    for(int i = 0; i < 5; i++){
        QString command = QString::number(i+1) + ',' \
                + QString::number(min_time) + ','\
                + QString::number(max_time);
        QString set_aloha = python_path + script + command;
        cout<<set_aloha.toStdString().c_str()<<endl;
        system(set_aloha.toStdString().c_str());
    }
    this->close();

}

uwaloha_resend_config::~uwaloha_resend_config()
{
    delete ui;
}
