#include "qpsk_config.h"
#include "ui_qpsk_config.h"
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>

extern QString python_path;

qpsk_config::qpsk_config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::qpsk_config)
{
    ui->setupUi(this);
    init_();
    connect(ui->btn_qpsk_config, SIGNAL(clicked(bool)), this, SLOT(set_qpsk_config()));
}
void qpsk_config::init_(){
    ui->le_qpsk_rxgain->setText("0.1");
    ui->le_qpsk_txgain->setText("0.02");
    QRegExp double_rx("(0[\.]{1}[0-9]{1,5})");
    ui->le_qpsk_rxgain->setValidator(new QRegExpValidator(double_rx, this));
    ui->le_qpsk_txgain->setValidator(new QRegExpValidator(double_rx, this));
    setWindowTitle("Mode Power Config");
}

void qpsk_config::set_qpsk_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    double tx_gain = ui->le_qpsk_txgain->text().toDouble();
    double rx_gain = ui->le_qpsk_rxgain->text().toDouble();
    for(int i = 0; i<5; i++){
        QString argv1 = QString::number(i+1) + ',' \
                + QString::number(rx_gain) + ','\
                + QString::number(tx_gain);
        QString script("./python_script/set_qpsk_rxgain_txgain.py ");
        QString command = python_path + script + argv1;
        cout<<command.toStdString().c_str()<<endl;
        system(command.toStdString().c_str());
    }
    this->close();
}

qpsk_config::~qpsk_config()
{
    delete ui;
}
