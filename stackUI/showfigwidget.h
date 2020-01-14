#ifndef SHOWFIGWIDGET_H
#define SHOWFIGWIDGET_H

#include <QWidget>
#include <QFileDialog>
#include <QMouseEvent>

class UserMainWindow;
namespace Ui {
class ShowFigWidget;
}

class ShowFigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShowFigWidget(UserMainWindow* ower, QWidget *parent = 0);
    void setOwerAndTarget(int ower, int target);
    ~ShowFigWidget();

public:
    void set_send_image_status(int status, int bodyIndex=0);
    void set_p_fig_widget_title(QString title);
    void init_fig_widget();
    void adapt_image(int width, int height);
    int get_header_wait_time();
    int get_packet_interval();
    UserMainWindow * ower_;
    void init_();

protected slots:
    void on_pushbuttonclose_clicked();
    void on_pushbuttonopen_clicked();
    void sendPicture();
    void on_pushbuttondraw_clicked();


protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

private slots:

private:
    Ui::ShowFigWidget *ui;
    int owerChatwindows;
    int targetNode;
    QString filename_;

    QImage drawImage, image;
    QPixmap pix;
    QPoint lastPoint;
    QPoint endPoint;
};

#endif // SHOWFIGWIDGET_H
