#ifndef RECV_IMAGE_H
#define RECV_IMAGE_H

#include <QDialog>
#include "app_datapackage.h"
#include <QFile>
class UserMainWindow;

namespace Ui {
class recv_image;
}

class recv_image : public QDialog
{
    Q_OBJECT

public:
    explicit recv_image(UserMainWindow* ower, QWidget *parent = 0);
    ~recv_image();

public:
    void recvimage(struct ImagePackage *p_package_recv);
    void handleImageHeader(struct ImageHeader *p_package_recv);
    void handleImageData(struct ImageData *p_package_recv);
    void handleImageHeaderAck(struct ImageHeader *p_package_recv);
    QString rs_decode_image(QString image_file);
    void adapt_widget_size(int width, int height);
    void init_();

public slots:
    void showTestPicture();
    void saveImage();
    void saveImageAS();
    void emptyTemp();
    void error_correct();




private:
    Ui::recv_image *ui;
    QImage *imageTemp = NULL;
    QFile *imageFileTemp;
    int tmpSourceID = 255;
    uint pictureIDTemp = 256;
    uint Code_K;
    UserMainWindow * ower_;
    uint16_t n;
    uint16_t k;

};

#endif // RECV_IMAGE_H
