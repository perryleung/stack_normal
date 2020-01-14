#ifndef SELECT_NODE_H
#define SELECT_NODE_H

#include <QDialog>

namespace Ui {
class select_node;
}

class select_node : public QDialog
{
    Q_OBJECT

public:
    explicit select_node(QWidget *parent = 0);
    ~select_node();
    void init_();
public slots:
    void start_a_stack_and_connect();

private:
    Ui::select_node *ui;
};

#endif // SELECT_NODE_H
