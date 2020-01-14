#ifndef FSK_1710_CONFIG_H
#define FSK_1710_CONFIG_H

#include <QDialog>

namespace Ui {
class fsk_1710_config;
}

class fsk_1710_config : public QDialog
{
    Q_OBJECT

public:
    explicit fsk_1710_config(QWidget *parent = 0);
    ~fsk_1710_config();
    void init_();
public slots:
    void set_fsk_1710_config();

private:
    Ui::fsk_1710_config *ui;
};

#endif // FSK_1710_CONFIG_H
