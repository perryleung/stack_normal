#include "config_panel_stack.h"
#include "ui_config_panel_stack.h"
#include <QMessageBox>
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include "config.h"
#include <QFile>

extern QString python_path;
QString default_item_text("select protocol");

config_panel_stack::config_panel_stack(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::config_panel_stack)
{
    ui->setupUi(this);
    srt_entry_config_ = new srt_entry_config();
    uwaloha_resend_config_ = new uwaloha_resend_config();
    qpsk_config_ = new qpsk_config();
    fsk_config_ = new fsk_config();
    fsk_1710_config_ = new fsk_1710_config();

    init_();
    connect(ui->cb_net, SIGNAL(currentIndexChanged(int)), this, SLOT(net_layer_config()));
    connect(ui->cb_mac, SIGNAL(currentIndexChanged(int)), this, SLOT(mac_layer_config()));
    connect(ui->cb_phy, SIGNAL(currentIndexChanged(int)), this, SLOT(phy_layer_config()));
    connect(ui->btn_compile, SIGNAL(clicked(bool)), this, SLOT(compile_stack()));

    get_current_roastFish_recipe();

}

void config_panel_stack::get_current_roastFish_recipe(QString recipe){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script("./python_script/get_current_roastfish_recipe.py");
    QString command = python_path + script;
    system(command.toStdString().c_str());
    cout<<command.toStdString().c_str()<<endl;



    QFile recipe_file(recipe);
    if(!recipe_file.open(QIODevice::ReadOnly)){
        QMessageBox::information(this,tr("Warning"),tr("Open recipe failed!"),QMessageBox::Ok);
        return;
    }

    for(int i = 0; i < 4; i++){
        QString temp= recipe_file.readLine();
        current_protocol_list[i]->setText(temp);
        cout<<temp<<endl;
    }


}

QString config_panel_stack::protocol_config(){
    QString module_config("");
    for(int i=0; i<cbo_protocol_list.size();i++){
        module_config += QString::number(cbo_protocol_list[i]->currentIndex());
        module_config += QString(",");
    }
    return module_config;
}
void config_panel_stack::compile_stack(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    for(int i = 0; i<cbo_protocol_list.size(); i++){
        if(cbo_protocol_list[i]->currentText() == default_item_text){
            QMessageBox::information(this,tr("Warning"),tr("Please Select A Protocol For Each Layer"),QMessageBox::Ok);
            return;
        }
        }

    QString module_config = protocol_config();
    cout<<"module config: "<<module_config.toStdString().c_str()<<endl;

    QString script("./python_script/gen_RoastFish.py ");
    QString command = python_path + script + module_config;
    get_current_roastFish_recipe();

//    ui->btn_compile->setText("Compling...");
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());
    get_current_roastFish_recipe();
}

void config_panel_stack::init_(){
    cbo_protocol_list.push_back(ui->cb_tra);
    cbo_protocol_list.push_back(ui->cb_net);
    cbo_protocol_list.push_back(ui->cb_mac);
    cbo_protocol_list.push_back(ui->cb_phy);

    current_protocol_list.push_back(ui->lab_current_tra);
    current_protocol_list.push_back(ui->lab_current_net);
    current_protocol_list.push_back(ui->lab_current_mac);
    current_protocol_list.push_back(ui->lab_current_phy);


    for(int i = 0; i<cbo_protocol_list.size(); i++){
        cbo_protocol_list[i]->addItem(default_item_text);
        cbo_protocol_list[i]->setCurrentText(default_item_text);
    }

    setWindowTitle("Stack Config");
}


void config_panel_stack::phy_layer_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    switch (ui->cb_phy->currentIndex()) {

    case 0:
        cout<<"select " <<ui->cb_phy->currentText().toStdString().c_str()<<endl;
        break;
    case 1:
        qpsk_config_->show();
        break;
    case 2:
        fsk_config_->show();
        break;
    case 3:
        fsk_1710_config_->show();
        break;
    default:
        break;
    }

}

void config_panel_stack::mac_layer_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    switch (ui->cb_mac->currentIndex()) {

    case 0:
        cout<<"select " <<ui->cb_mac->currentText().toStdString().c_str()<<endl;
        break;
    case 1:
        uwaloha_resend_config_->show();
        break;
    case 2:
        cout<<"select " << ui->cb_mac->currentText().toStdString().c_str()<<endl;
        break;
    default:
        break;
    }
}


void config_panel_stack::net_layer_config(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    switch (ui->cb_net->currentIndex()) {
    case 0:
        srt_entry_config_->show();
        break;
    case 1:
        cout<<"select drt"<<endl;
        break;
    case 2:
        cout<<"select vbf"<<endl;
        break;
    default:
        break;
    }
}

config_panel_stack::~config_panel_stack()
{
    delete ui;
}
