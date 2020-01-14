#ifndef UWALOHA_RESEND_CONFIG_H
#define UWALOHA_RESEND_CONFIG_H

#include <QDialog>

namespace Ui {
class uwaloha_resend_config;
}

class uwaloha_resend_config : public QDialog
{
    Q_OBJECT

public:
    explicit uwaloha_resend_config(QWidget *parent = 0);
    ~uwaloha_resend_config();
    void init_();
public slots:
    void set_uwaloha_resend_config();

private:
    Ui::uwaloha_resend_config *ui;
};

#endif // UWALOHA_RESEND_CONFIG_H
