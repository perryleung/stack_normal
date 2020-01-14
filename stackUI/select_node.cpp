#include "select_node.h"
#include "ui_select_node.h"
#include "usermainwindow.h"
#include "pcommunication.h"
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>


extern QString python_path;

select_node::select_node(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::select_node)
{
    ui->setupUi(this);
    init_();
    connect(ui->btn_connect, SIGNAL(clicked(bool)), this, SLOT(start_a_stack_and_connect()));
}

void select_node::init_(){
    setWindowTitle("Input Node ID");
    ui->le_node_id->setValidator(new QIntValidator(1, 5, this));
}

void select_node::start_a_stack_and_connect(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    int input_node_id = ui->le_node_id->text().toInt();

    QString script("./python_script/start_a_node.py ");
    script += QString::number(input_node_id);
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());
    sleep(0.5);


    UserMainWindow * user = new UserMainWindow(input_node_id);
    user->transform_user();
//    user->set_self_nodeid(input_node_id);
    PCommunication::getInstance()->user_list.push_back(user);
    user->connectServer();
    user->show();
}

select_node::~select_node()
{
    delete ui;
}
