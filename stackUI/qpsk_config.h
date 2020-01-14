#ifndef QPSK_CONFIG_H
#define QPSK_CONFIG_H

#include <QDialog>

namespace Ui {
class qpsk_config;
}

class qpsk_config : public QDialog
{
    Q_OBJECT

public:
    explicit qpsk_config(QWidget *parent = 0);
    ~qpsk_config();
    void init_();
public slots:
    void set_qpsk_config();
private:
    Ui::qpsk_config *ui;
};

#endif // QPSK_CONFIG_H
