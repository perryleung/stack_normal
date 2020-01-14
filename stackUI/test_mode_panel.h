#ifndef TEST_MODE_PANEL_H
#define TEST_MODE_PANEL_H

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QTcpSocket>
#include <QLabel>


namespace Ui {
class test_mode_panel;
}

class test_mode_panel : public QDialog
{
    Q_OBJECT

public:
    explicit test_mode_panel(QWidget *parent = 0);
    ~test_mode_panel();
    void start_a_test_node(int node_id, int delay);
    void set_a_node_packet_interval(int node_id);
    void set_a_node_packet_num(int node_id);
    void init_();
public slots:
    void start_test_mode();
    void stop_test_mode();
    void check_nodes_status();
    void show_test_log();
private:
    Ui::test_mode_panel *ui;
    std::vector<QCheckBox*> node_list;
    std::vector<QLineEdit*> delay_list;
    std::vector<QLineEdit*> packet_num_list;
    std::vector<QLabel *> nodes_status_list;
    QTcpSocket *check_socket;



};

#endif // TEST_MODE_PANEL_H
