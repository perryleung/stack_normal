#include "send_img_dwt_fountain.h"
#include "ui_send_img_dwt_fountain.h"
#include <QMessageBox>
#include <QDebug>
#include <QFileDialog>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include "app_datapackage.h"

QString ana_python("/home/ucc/anaconda2/bin/ipython ");
send_img_dwt_fountain::send_img_dwt_fountain(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::send_img_dwt_fountain)
{
    ui->setupUi(this);
    init_();
    connect(ui->pushButton_open, SIGNAL(clicked(bool)),  this, SLOT(open_image()));
    connect(ui->pushButton_send,SIGNAL(clicked(bool)), this, SLOT(fountain_connect()));
    connect(ui->btn_ready,SIGNAL(clicked(bool)), this, SLOT(qt_fountain_manager()));
    connect(ui->pushButton_close, SIGNAL(clicked(bool)), this, SLOT(close_()));
//    connect(this->fountain_socket_, SIGNAL(connected()), this, SLOT(fountain_connect()));
    connect(this->fountain_socket_, SIGNAL(readyRead()), this, SLOT(drop2packet()));
}

void send_img_dwt_fountain::init_(){
    adapt_image(100, 100);
    QRegExp double_less_1("(0[\.]{1}[0-9]{1,5})");
    ui->le_header_wait_time->setText(QString::number(100));
    ui->le_packet_interval->setText(QString::number(20));
    ui->le_header_wait_time->setEnabled(true);
    ui->le_packet_interval->setEnabled(true);
    ui->le_header_wait_time->setValidator(new QIntValidator(0, 1000, this));
    ui->le_packet_interval->setValidator(new QIntValidator(0, 1000, this));
    ui->le_w1_size->setPlaceholderText("size");
    ui->le_w1_pro->setPlaceholderText("pro");
    ui->le_w1_size->setText(QString::number(0.3));
    ui->le_w1_pro->setText(QString::number(0.5));
    ui->le_w1_size->setValidator(new QRegExpValidator(double_less_1, this));
    ui->le_w1_pro->setValidator(new QRegExpValidator(double_less_1, this));
    fountain_socket_= new QTcpSocket(this);
}

send_img_dwt_fountain::~send_img_dwt_fountain()
{
    delete ui;
}
void send_img_dwt_fountain::show_(){
    this->show();
}

void send_img_dwt_fountain::close_(){
    this->close();
}

void send_img_dwt_fountain::qt_fountain_manager(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    QString img_file = filename_;
    QString interval =ui->le_packet_interval->text();
    QString w1_size = ui->le_w1_size->text();
    QString w1_pro = ui->le_w1_pro->text();
    QString port = QString::number(9999);
    port_ = port;

    QString script("./python_script/qt_fountain_manager.py send ");
    QString argv = img_file+","+interval+","+w1_size+","+w1_pro+","+port+","+QString::number(PICTUERDATA_PACKAGE_SIZE);
    QString command = ana_python + script + argv;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

}

void send_img_dwt_fountain::drop2packet(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    time_t now;
    time(&now);
//  8类型的数据包不需要重传，7类型的数据包需要重传
    sendImageData.Package_type = 8;
    sendImageHeader.Package_type = 7;
    sendImageData.SourceID = sendImageHeader.SourceID = owerChatwindows;
    sendImageData.DestinationID = sendImageHeader.DestinationID = targetNode;
    sendImageData.PictureID = sendImageHeader.PictureID = now % 255;

    int drop_len = fountain_socket_->bytesAvailable();
    char readBuff[drop_len];
    cout<<"read bytes : "<<drop_len<<endl;
    fountain_socket_->read(readBuff, drop_len);

    memcpy(sendImageData.data_buffer, readBuff, drop_len);
    bzero(can, sizeof(can));
    memcpy(can, &sendImageData, sizeof(ImageData));
    stack_socket_->write(can, sizeof(can));
}

void send_img_dwt_fountain::set_stack_socket(QTcpSocket *socket_){
    stack_socket_ = socket_;
}

void send_img_dwt_fountain::fountain_connect(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    fountain_socket_->connectToHost(QHostAddress("127.0.0.1"), port_.toInt());
}


void send_img_dwt_fountain::open_image(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    cout<<"open file"<<endl;

    QString fileName=QFileDialog::getOpenFileName(this, tr("open file"), QDir::currentPath(), tr(""));//QDir::homePath()
    if(fileName.isEmpty())
    {
        QMessageBox::information(this,tr("Warning"),tr("Open Image file failed! fileName is NULL."),QMessageBox::Ok);
        return;
    }
    image = QImage(fileName);
    filename_ = fileName;
    adapt_image(image.width(), image.height());


    pix = QPixmap(image.width(), image.height());
    pix = QPixmap::fromImage(image);
    ui->label->setPixmap(QPixmap::fromImage(image));
}



void send_img_dwt_fountain::adapt_image(int width, int height){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    cout<<width<<" : "<<height<<endl;

    if(height>170){
        this->setGeometry(0, 0, width + 300, height + 30 + 80);
        ui->label_send_image_status->setGeometry(10, height + 10, width + 260, height + 20);
        ui->te_send_status->setGeometry(10, height + 20 + 20 + 5, width + 70, 40);

    } else {
        this->setGeometry(0, 0, width + 270, 170 + 40 + 100);
        ui->label_send_image_status->setGeometry(10, 170, width + 260, 20);
        ui->te_send_status->setGeometry(10, 200, width + 250, 80);
    }

//    set_send_image_status(0);
    ui->label->setGeometry(10, 10, width, height);
    ui->pushButton_open->setGeometry(width + 40, 10, 100, 30);

    ui->lab_header_wait_time->setGeometry(width + 40, 40, 120, 30);
    ui->le_header_wait_time->setGeometry(width + 170, 40, 80, 30);

    ui->lab_packet_interval->setGeometry(width + 40, 70, 120, 30);
    ui->le_packet_interval->setGeometry(width + 170, 70, 80, 30);

    ui->lab_w1_para->setGeometry(width + 40, 100, 80, 30);
    ui->le_w1_size->setGeometry(width + 130, 100, 60, 30);
    ui->le_w1_pro->setGeometry(width + 190, 100, 60, 30);

    ui->btn_ready->setGeometry(width, 140, 60, 30);
    ui->pushButton_send->setGeometry(width+ 80, 140, 60, 30);
    ui->pushButton_close->setGeometry(width + 140, 140, 60, 30);
}


void send_img_dwt_fountain::setOwerAndTarget(int ower, int target){
    owerChatwindows = ower;
    targetNode = target;
}

void send_img_dwt_fountain::set_ower(UserMainWindow *ower){
    ower_ = ower;
}
