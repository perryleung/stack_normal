#include "showfigwidget.h"
#include "ui_showfigwidget.h"
#include <QMessageBox>
#include <QImage>
#include <QDebug>
#include <QPoint>
#include <QPixmap>
#include <QPainter>
#include <iostream>
#include <QTextStream>
#include <QString>
#include <QDateTime>
#include "app_datapackage.h"
#include "pcommunication.h"
#include <QColor>


ShowFigWidget::ShowFigWidget(UserMainWindow *ower, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShowFigWidget)
{
//    setWindowFlags(Qt::FramelessWindowHint);
    ui->setupUi(this);
    ower_ = ower;
    init_fig_widget();
    init_();

    connect(ui->pushButton_close,SIGNAL(clicked(bool)),this,SLOT(on_pushbuttonclose_clicked()));
    connect(ui->pushButton_open,SIGNAL(clicked(bool)),this,SLOT(on_pushbuttonopen_clicked()));
    connect(ui->pushButton_send, SIGNAL(clicked(bool)), this, SLOT(sendPicture()));    
}

ShowFigWidget::~ShowFigWidget()
{
    delete ui;
}

void ShowFigWidget::init_(){
    ui->te_send_status->setReadOnly(true);
}

int ShowFigWidget::get_header_wait_time(){
    return ui->le_header_wait_time->text().toInt();
}

int ShowFigWidget::get_packet_interval(){
    return ui->le_packet_interval->text().toInt();
}

void ShowFigWidget::adapt_image(int width, int height){
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
    ui->lab_header_wait_time->setGeometry(width + 40, 50, 120, 30);
    ui->le_header_wait_time->setGeometry(width + 170, 50, 80, 30);
    ui->lab_packet_interval->setGeometry(width + 40, 90, 120, 30);
    ui->le_packet_interval->setGeometry(width + 170, 90, 80, 30);
    ui->pushButton_send->setGeometry(width+ 40, 120, 100, 30);
    ui->pushButton_close->setGeometry(width + 150, 120, 100, 30);
}

void ShowFigWidget::init_fig_widget(){
    adapt_image(100, 100);
    ui->le_header_wait_time->setText(QString::number(100));
    ui->le_packet_interval->setText(QString::number(20));
    ui->le_header_wait_time->setEnabled(true);
    ui->le_packet_interval->setEnabled(true);
    ui->le_header_wait_time->setValidator(new QIntValidator(0, 1000, this));
    ui->le_packet_interval->setValidator(new QIntValidator(0, 1000, this));
}


void ShowFigWidget::set_p_fig_widget_title(QString title){
    setWindowTitle(title);
}

void ShowFigWidget::on_pushbuttonclose_clicked()
{
    this->close();
//    this->~ShowFigWidget();
}

void ShowFigWidget::on_pushbuttonopen_clicked()
{
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



void ShowFigWidget::set_send_image_status(int status, int bodyIndex){
    //0:idle,1:send header, 2:send body, 3:send rs, 4:error
    QString str("send status:\n");
    QString idle("idle");
    QString sending_header("sending header");
    QString sending_body("recv header and sending body");
    QString doing(" ...\n");
    QString done(" done\n");
    QString not_yet(" \n");
    QString current_status = ui->te_send_status->toPlainText();
    switch(status){
    case 0:{
//        ui->label_send_image_status->setText(str + idle + doing + sending_header + not_yet + sending_body + not_yet);
        ui->te_send_status->setText(current_status + QString("\n idle"));
        break;
    }
    case 1:{
//        ui->label_send_image_status->setText(str + idle + done + sending_header + doing + sending_body + not_yet);
        ui->te_send_status->setText(current_status + QString("\n send header"));
        break;
    }
    case 2:{
//        ui->label_send_image_status->setText(str + idle + done + sending_header + done + sending_body + doing);
        QString bodyIndexS = QString::number(bodyIndex);
        ui->te_send_status->setText(current_status + QString("\n send body packet: ")+ bodyIndexS);
        break;
    }
    case 3:{
        ui->label_send_image_status->setText(str+"sending rs");
        break;
    }
    case 4:{
//        ui->label_send_image_status->setText(str+"header no respone and quit sending");
        ui->te_send_status->setText(current_status + QString("\nheader no respone and quit sending"));
        break;
    }

    }
    QTextCursor cursor = ui->te_send_status->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->te_send_status->setTextCursor(cursor);

}


void ShowFigWidget::paintEvent(QPaintEvent *){
    QPainter pp(&pix);
    pp.setPen(QPen(Qt::magenta,2,Qt::DashDotLine, Qt::RoundCap));
    pp.drawLine(lastPoint, endPoint);
    lastPoint = endPoint;
    QPainter painter(this);
    painter.drawPixmap(10, 10, pix);

}

void ShowFigWidget::mousePressEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        QPoint posDiff(-10,-10);
        lastPoint = event->pos() + posDiff;
//        ui->label->setPixmap(pix);
    }
}

void ShowFigWidget::mouseMoveEvent(QMouseEvent *event){
    if(event->buttons() & Qt::LeftButton){
        QPoint posDiff(-10,-10);
        endPoint = event->pos() + posDiff;
        update();
        ui->label->setPixmap(pix);
    }
}
void ShowFigWidget::mouseReleaseEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        endPoint = event->pos();
        update();
        ui->label->setPixmap(pix);
    }

}

void ShowFigWidget::setOwerAndTarget(int ower, int target){
    owerChatwindows = ower;
    targetNode = target;
}
void ShowFigWidget::sendPicture(){
    drawImage = pix.toImage();
    drawImage.save(filename_+QString("draw.jpg"));
    filename_ = filename_+QString("draw.jpg");
    ui->le_header_wait_time->setEnabled(false);
    ui->le_packet_interval->setEnabled(false);

    ower_->sendPicture(filename_, targetNode, owerChatwindows);
//    PCommunication::getInstance()->p_UserMainWindow->sendPixPicture(filename_, targetNode, owerChatwindows);
//    PCommunication::getInstance()->p_UserMainWindow->sendQimagePicture(filename_, targetNode, owerChatwindows);
}


