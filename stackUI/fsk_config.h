#ifndef FSK_CONFIG_H
#define FSK_CONFIG_H

#include <QDialog>

namespace Ui {
class fsk_config;
}

class fsk_config : public QDialog
{
    Q_OBJECT

public:
    explicit fsk_config(QWidget *parent = 0);
    ~fsk_config();
    void init_();
public slots:
    void set_fsk_config();

private:
    Ui::fsk_config *ui;
};

#endif // FSK_CONFIG_H
