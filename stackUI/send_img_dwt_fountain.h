#ifndef SEND_IMG_DWT_FOUNTAIN_H
#define SEND_IMG_DWT_FOUNTAIN_H

#include <QDialog>
#include<QtNetwork>
#include "app_datapackage.h"

class UserMainWindow;
namespace Ui {
class send_img_dwt_fountain;
}

class send_img_dwt_fountain : public QDialog
{
    Q_OBJECT

public:
    explicit send_img_dwt_fountain(QWidget *parent = 0);
    ~send_img_dwt_fountain();
    void init_();
    void adapt_image(int width, int height);
    void show_();
    void connect_fountain();
    void setOwerAndTarget(int ower, int target);
    void set_ower(UserMainWindow * ower);
    void set_stack_socket(QTcpSocket *client_socket);
public slots:
    void open_image();
    void qt_fountain_manager();
    void close_();
    void fountain_connect();
    void drop2packet();

private:
    Ui::send_img_dwt_fountain *ui;
    QString filename_;
    QImage drawImage, image;
    QPixmap pix;
    QTcpSocket *fountain_socket_;
    QTcpSocket *stack_socket_;
    QString port_;
    struct ImageHeader sendImageHeader;
    struct ImageData sendImageData;
    int owerChatwindows;
    int targetNode;
    char can[sizeof(ImageData)];
    UserMainWindow * ower_;


};

#endif // SEND_IMG_DWT_FOUNTAIN_H

