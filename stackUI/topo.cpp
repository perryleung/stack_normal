#include "topo.h"
#include "ui_topo.h"
#include "pcommunication.h"
#include <QPainter>
#include <math.h>
#include <QMessageBox>
#include <iostream>
#include <string>
#include <regex>
#include <time.h>
#include <cstdlib>
#include <QRegExp>
#include <QString>


uint8_t topoTest[2][20] = {{127}, {127}};
uint8_t * test = &topoTest[0][0];
Topo::Topo(UserMainWindow * ower, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Topo)
{
    ui->setupUi(this);
    ower_ = ower;
    setWindowTitle(QString("node ") + QString::number(ower_->GetSelfNodeID()) + QString(" information"));
    setTopo(test);
}

Topo::~Topo()
{
    delete ui;
}

void Topo::set_info_window_id(int node_id){
    setWindowTitle(QString("node ") + QString::number(ower_->GetSelfNodeID()) + QString(" information"));
}


void Topo::paintEvent(QPaintEvent *)
{
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;


    QPainter painter(this);
    QPen pen, penText;
    pen.setColor(QColor(11, 45, 100));


    QBrush brush(QColor(11, 45, 100));
    painter.setPen(pen);
    painter.setBrush(brush);
    penText.setColor(QColor(255, 255, 255));


    painter.drawEllipse(QPoint(200, 200), 30, 30);
    painter.setPen(penText);
    painter.drawText(200 - 20, 200 + 0, QString("node ") + QString::number(ower_->GetSelfNodeID()));

    penText.setColor(QColor(0, 0, 0));
    painter.setPen(penText);


    uint8_t * foruse = getTopo();
    double * p = calElic();
    drawTopo(p);


}

uint8_t * Topo::getTopo()
{
    return topo;
}

void Topo::setTopo(uint8_t *a)
{
    memcpy(topo, a, PACKAGE_SIZE);
}

double * Topo::calElic()
{
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    int chore = 150;
    int len = 0;
    for(int i = 0; ; i++){
        if(topo[i] == 127)
            break;
        len++;

   }
   cout<<"len = "<<len<<endl;
   double angle = 3.14159 * 2 / len;
   double *point = new double[PACKAGE_SIZE];
   bzero(point, PACKAGE_SIZE);
   for(int i = 0; i<len; i++)
   {
       point[i] = 200 + chore * cos(angle*i);
       (point + PACKAGE_SIZE / 2)[i] = 200 + chore * sin(angle*i);
   }

   return point;
}

void Topo::drawTopo(double *point)
{
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    QPainter painter(this);
    QPen penElli, penSoil, penDash, penText;
    penElli.setColor(QColor(11, 45, 100));
    penSoil.setColor(QColor(11, 45, 100));
    penDash.setColor(QColor(11, 45, 100));
    penText.setColor(QColor(255, 255, 255));

    penSoil.setStyle(Qt::SolidLine);
    penDash.setStyle(Qt::DashLine);


    QBrush brush(QColor(11, 45, 100));

    int len = 0;
    for(int i = 0; ; i++){
        if(topo[i] == 127)
            break;
        len++;
   }
   for(int i = 0; i < len; i++){
       painter.setPen(penElli);
       painter.setBrush(brush);
       painter.drawEllipse(QPoint(point[i], (point + PACKAGE_SIZE / 2)[i]), 30, 30);
       if((topo + PACKAGE_SIZE / 2)[i])
           painter.setPen(penSoil);
       else
           painter.setPen(penDash);
       painter.drawLine(QPoint(200, 200), QPoint(point[i], (point + PACKAGE_SIZE / 2)[i]));
       painter.setPen(penText);
       painter.drawText(point[i] - 22, (point + PACKAGE_SIZE / 2)[i] + 2, QString("node ") + QString::number(topo[i]));
   }
   cout<<"drawTopo finish"<<endl;
}
