#include "fsk_config.h"
#include "ui_fsk_config.h"
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>

extern QString python_path;

fsk_config::fsk_config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::fsk_config)
{
    ui->setupUi(this);
    connect(ui->btn_fsk_config, SIGNAL(clicked(bool)), this, SLOT(set_fsk_config()));
    init_();
}

void fsk_config::init_(){
    setWindowTitle("FSK Config");
    QRegExp double_gain("(0[\.]{1}[0-9]{1,5})");
    setWindowTitle("MFSK Gain");
    ui->le_fsk_gain->setText("0.02");
    ui->le_fsk_gain->setValidator(new QRegExpValidator(double_gain, this));
    ui->le_fsk_tx_gain->setText("0.005");
    ui->le_fsk_tx_gain->setValidator(new QRegExpValidator(double_gain, this));
}

void fsk_config::set_fsk_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    double gain = ui->le_fsk_gain->text().toDouble();
    double tx_gin = ui->le_fsk_tx_gain->text().toDouble();
    for(int i = 0; i<5; i++){
        QString argv1 = QString::number(i+1) + ',' \
                + QString::number(gain) + ',' + QString::number(tx_gin);
        QString script("./python_script/set_fsk_gain.py ");
        QString command = python_path + script + argv1;
        cout<<command.toStdString().c_str()<<endl;
        system(command.toStdString().c_str());
    }
    this->close();
}

fsk_config::~fsk_config()
{
    delete ui;
}
