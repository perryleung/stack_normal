#include "test_mode_panel.h"
#include "ui_test_mode_panel.h"
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>


extern QString python_path;
QString python2_path("python ");

test_mode_panel::test_mode_panel(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::test_mode_panel)
{
    ui->setupUi(this);
    connect(ui->btn_start_test, SIGNAL(clicked(bool)), this, SLOT(start_test_mode()));
    connect(ui->btn_stop_test, SIGNAL(clicked(bool)), this, SLOT(stop_test_mode()));
    connect(ui->btn_check_nodes_status, SIGNAL(clicked(bool)), this, SLOT(check_nodes_status()));
    connect(ui->btn_show_log, SIGNAL(clicked(bool)), this, SLOT(show_test_log()));
    init_();
}

void test_mode_panel::init_(){
    int delay[5] = {0, 45, 110, 190};
    ui->btn_show_log->close();
    setWindowTitle("Test Config");
    node_list.push_back(ui->cb_node_1);
    node_list.push_back(ui->cb_node_2);
    node_list.push_back(ui->cb_node_3);
    node_list.push_back(ui->cb_node_4);
    node_list.push_back(ui->cb_node_5);

    delay_list.push_back(ui->le_node_delay_1);
    delay_list.push_back(ui->le_node_delay_2);
    delay_list.push_back(ui->le_node_delay_3);
    delay_list.push_back(ui->le_node_delay_4);
    delay_list.push_back(ui->le_node_delay_5);

    packet_num_list.push_back(ui->le_packet_num_1);
    packet_num_list.push_back(ui->le_packet_num_2);
    packet_num_list.push_back(ui->le_packet_num_3);
    packet_num_list.push_back(ui->le_packet_num_4);
    packet_num_list.push_back(ui->le_packet_num_5);

    nodes_status_list.push_back(ui->lab_node_status_1);
    nodes_status_list.push_back(ui->lab_node_status_2);
    nodes_status_list.push_back(ui->lab_node_status_3);
    nodes_status_list.push_back(ui->lab_node_status_4);
    nodes_status_list.push_back(ui->lab_node_status_5);

    setWindowTitle("Test Mode Config");
    for(int i = 0; i<node_list.size(); i++){
        delay_list[i]->setText(QString::number(delay[i]));
        delay_list[i]->setValidator(new QIntValidator(0, 500, this));
        packet_num_list[i]->setValidator(new QIntValidator(0, 1000, this));
        packet_num_list[i]->setText("100");
        nodes_status_list[i]->setText("unknow");

    }
    ui->le_packet_interval_max->setText("50");
    ui->le_packet_interval_min->setText("10");
    ui->le_packet_interval_min->setValidator(new QIntValidator(0, 500, this));
    ui->le_packet_interval_max->setValidator(new QIntValidator(0, 500, this));

}

void test_mode_panel::check_nodes_status(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    for(int i = 0; i<nodes_status_list.size(); i++){
        QString host("192.168.2.10");
        host += QString::number(i+1);
        check_socket = new QTcpSocket(this);

        check_socket->connectToHost(host, 80);
        if(check_socket->waitForConnected(500)){
            cout<<host.toStdString().c_str()<<"okay"<<endl;
            nodes_status_list[i]->setText("pass");
        } else {
            cout<<host.toStdString().c_str()<<"fail"<<endl;
            nodes_status_list[i]->setText("fail");
        }
        check_socket->close();
    }

}

void test_mode_panel::stop_test_mode(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script("./python_script/kill_all_RoastFish.py ");
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

}

void test_mode_panel::set_a_node_packet_num(int node_id){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    int packet_num = packet_num_list[node_id-1]->text().toInt();
    QString script("./python_script/set_test_packet_num.py ");
    script += QString::number(node_id);
    script += ',';
    script += QString::number(packet_num);
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

}

void test_mode_panel::set_a_node_packet_interval(int node_id){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    int min_t = ui->le_packet_interval_min->text().toInt();
    int max_t = ui->le_packet_interval_max->text().toInt();
    QString script("./python_script/set_test_packet_interval.py ");
    script += QString::number(node_id);
    script += ',';
    script += QString::number(min_t);
    script += ',';
    script += QString::number(max_t);
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

}

void test_mode_panel::show_test_log(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script("../performance_analysis/tracehandler.py ");
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());
}

void test_mode_panel::start_a_test_node(int node_id, int delay){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script("./python_script/start_a_test_node.py ");
    script += QString::number(node_id);
    script += ",";
    script += QString::number(delay + 2);
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

}
void test_mode_panel::start_test_mode(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script_start_trace_handel_server("./python_script/show_test_log.py ");
    QString command1 = python_path + script_start_trace_handel_server;
    cout<<command1.toStdString().c_str()<<endl;
    system(command1.toStdString().c_str());

    cout<<"start_test_mode"<<endl;
//    int start_nodes_num = 0;
    int max_delay = 0;
    for(int i=0; i<node_list.size(); i++){
        if(node_list[i]->isChecked())
            if(delay_list[i]->text().toInt() > max_delay)
                max_delay = delay_list[i]->text().toInt();
//            start_nodes_num += 1;
    }
    QString script("./python_script/set_test_time_for_handshake.py ");
    script += QString::number(max_delay + 40);
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());

    for(int i=0; i<node_list.size(); i++){
        cout<<i<<endl;
        if(node_list[i]->isChecked()){
            cout<<"check "<<i+1<<endl;
            int delay = delay_list[i]->text().toInt();
            set_a_node_packet_num(i+1);
            set_a_node_packet_interval(i+1);
            start_a_test_node(i+1, delay);
        } else {

        }
    }
}

test_mode_panel::~test_mode_panel()
{
    delete ui;
}
