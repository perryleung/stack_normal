#include "fsk_1710_config.h"
#include "ui_fsk_1710_config.h"
#include <iostream>
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>

extern QString python_path;

fsk_1710_config::fsk_1710_config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::fsk_1710_config)
{
    ui->setupUi(this);
    connect(ui->btn_save_close, SIGNAL(clicked(bool)), this, SLOT(set_fsk_1710_config()));
    init_();
}

void fsk_1710_config::init_(){
    setWindowTitle("MFSK Gain");
    QRegExp double_gain("(0[\.]{1}[0-9]{1,5})");
    ui->le_rx_gain->setText("0.2");
    ui->le_tx_gain->setText("0.005");
    ui->le_data_size->setText("128");

    ui->le_rx_gain->setValidator(new QRegExpValidator(double_gain, this));
    ui->le_tx_gain->setValidator(new QRegExpValidator(double_gain, this));
    ui->le_data_size->setValidator(new QIntValidator(1, 1024, this));

}

void fsk_1710_config::set_fsk_1710_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    double gain = ui->le_rx_gain->text().toDouble();
    double tx_gin = ui->le_tx_gain->text().toDouble();
    int data_size = ui->le_data_size->text().toInt();

    for(int i = 0; i<5; i++){
        QString argv1 = QString::number(i+1) + ',' \
                + QString::number(gain) + ',' \
                + QString::number(tx_gin) + ','\
                + QString::number(data_size);
        QString script("./python_script/set_fsk1710_config.py ");
        QString command = python_path + script + argv1;
        cout<<command.toStdString().c_str()<<endl;
        system(command.toStdString().c_str());
    }
    this->close();
}

fsk_1710_config::~fsk_1710_config()
{
    delete ui;
}
