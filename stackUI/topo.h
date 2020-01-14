#ifndef TOPO_H
#define TOPO_H

#include <QDialog>
#include "app_datapackage.h"
#include <string>
//#include <>

class UserMainWindow;

namespace Ui {
class Topo;
}

class Topo : public QDialog
{
    Q_OBJECT

public:
    explicit Topo(UserMainWindow * ower, QWidget *parent = 0);
    ~Topo();

    void set_info_window_id(int node_id);

private:
    Ui::Topo *ui;
protected:
    void paintEvent(QPaintEvent *);




private:
    uint8_t topo[PACKAGE_SIZE];
    UserMainWindow* ower_;



public:
    uint8_t * getTopo();
    void setTopo(uint8_t * a);
    double * calElic();
    void drawTopo(double* point);


};

#endif // TOPO_H
