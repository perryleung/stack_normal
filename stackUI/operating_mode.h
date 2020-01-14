#ifndef OPERATING_MODE_H
#define OPERATING_MODE_H

#include <QDialog>
#include "test_mode_panel.h"
#include "select_node.h"

namespace Ui {
class operating_mode;
}

class operating_mode : public QDialog
{
    Q_OBJECT

public:
    explicit operating_mode(QWidget *parent = 0);
    ~operating_mode();

public slots:
    void show_test_mode_panel();
    void show_chat_mode();

private:
    Ui::operating_mode *ui;
    test_mode_panel * test_mode_panel_;
    select_node * select_node_;
};

#endif // OPERATING_MODE_H
