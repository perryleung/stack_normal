#include "srt_entry_config.h"
#include "ui_srt_entry_config.h"
#include <vector>
#include <QDebug>
#include <unistd.h>
#include <string>
#include <stdlib.h>



extern QString python_path;

srt_entry_config::srt_entry_config(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::srt_entry_config)
{
    ui->setupUi(this);
    init_();
    connect(ui->btn_set_srt_entrys, SIGNAL(clicked(bool)), this, SLOT(set_srt_entrys()));

}

void srt_entry_config::set_srt_entrys(){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    for(int j = 0; j < 5; j++){
        int node_id = j + 1;
        QString command("\"");
        for(int i = 0; i < 4 ; i++){
            command += QString::number(cb_dest_list[(node_id - 1) * 4 + i]->currentIndex()+1) + ",";
            command += QString::number(cb_next_list[(node_id - 1) * 4 + i]->currentIndex()+1) + ",";
            command += QString::number(cb_hupnum_list[(node_id - 1) * 4 + i]->currentIndex()+1) + ";";
        }
        command += "\"";
        command = QString::number(node_id) + " " + command;
        QString script("./python_script/set_srt_entrys.py ");
        QString set_str_entrys = python_path + script + command;
        cout<<set_str_entrys.toStdString().c_str()<<endl;
        system(set_str_entrys.toStdString().c_str());
    }
    this->close();
}

void srt_entry_config::init_(){

    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    setWindowTitle(" Srt Entrys Config ");
    cb_dest_list.push_back(ui->cb_dest_1_1);
    cb_dest_list.push_back(ui->cb_dest_1_2);
    cb_dest_list.push_back(ui->cb_dest_1_3);
    cb_dest_list.push_back(ui->cb_dest_1_4);
    cb_dest_list.push_back(ui->cb_dest_2_1);
    cb_dest_list.push_back(ui->cb_dest_2_2);
    cb_dest_list.push_back(ui->cb_dest_2_3);
    cb_dest_list.push_back(ui->cb_dest_2_4);
    cb_dest_list.push_back(ui->cb_dest_3_1);
    cb_dest_list.push_back(ui->cb_dest_3_2);
    cb_dest_list.push_back(ui->cb_dest_3_3);
    cb_dest_list.push_back(ui->cb_dest_3_4);
    cb_dest_list.push_back(ui->cb_dest_4_1);
    cb_dest_list.push_back(ui->cb_dest_4_2);
    cb_dest_list.push_back(ui->cb_dest_4_3);
    cb_dest_list.push_back(ui->cb_dest_4_4);
    cb_dest_list.push_back(ui->cb_dest_5_1);
    cb_dest_list.push_back(ui->cb_dest_5_2);
    cb_dest_list.push_back(ui->cb_dest_5_3);
    cb_dest_list.push_back(ui->cb_dest_5_4);

    cb_next_list.push_back(ui->cb_next_1_1);
    cb_next_list.push_back(ui->cb_next_1_2);
    cb_next_list.push_back(ui->cb_next_1_3);
    cb_next_list.push_back(ui->cb_next_1_4);
    cb_next_list.push_back(ui->cb_next_2_1);
    cb_next_list.push_back(ui->cb_next_2_2);
    cb_next_list.push_back(ui->cb_next_2_3);
    cb_next_list.push_back(ui->cb_next_2_4);
    cb_next_list.push_back(ui->cb_next_3_1);
    cb_next_list.push_back(ui->cb_next_3_2);
    cb_next_list.push_back(ui->cb_next_3_3);
    cb_next_list.push_back(ui->cb_next_3_4);
    cb_next_list.push_back(ui->cb_next_4_1);
    cb_next_list.push_back(ui->cb_next_4_2);
    cb_next_list.push_back(ui->cb_next_4_3);
    cb_next_list.push_back(ui->cb_next_4_4);
    cb_next_list.push_back(ui->cb_next_5_1);
    cb_next_list.push_back(ui->cb_next_5_2);
    cb_next_list.push_back(ui->cb_next_5_3);
    cb_next_list.push_back(ui->cb_next_5_4);

    cb_hupnum_list.push_back(ui->cb_hupnum_1_1);
    cb_hupnum_list.push_back(ui->cb_hupnum_1_2);
    cb_hupnum_list.push_back(ui->cb_hupnum_1_3);
    cb_hupnum_list.push_back(ui->cb_hupnum_1_4);
    cb_hupnum_list.push_back(ui->cb_hupnum_2_1);
    cb_hupnum_list.push_back(ui->cb_hupnum_2_2);
    cb_hupnum_list.push_back(ui->cb_hupnum_2_3);
    cb_hupnum_list.push_back(ui->cb_hupnum_2_4);
    cb_hupnum_list.push_back(ui->cb_hupnum_3_1);
    cb_hupnum_list.push_back(ui->cb_hupnum_3_2);
    cb_hupnum_list.push_back(ui->cb_hupnum_3_3);
    cb_hupnum_list.push_back(ui->cb_hupnum_3_4);
    cb_hupnum_list.push_back(ui->cb_hupnum_4_1);
    cb_hupnum_list.push_back(ui->cb_hupnum_4_2);
    cb_hupnum_list.push_back(ui->cb_hupnum_4_3);
    cb_hupnum_list.push_back(ui->cb_hupnum_4_4);
    cb_hupnum_list.push_back(ui->cb_hupnum_5_1);
    cb_hupnum_list.push_back(ui->cb_hupnum_5_2);
    cb_hupnum_list.push_back(ui->cb_hupnum_5_3);
    cb_hupnum_list.push_back(ui->cb_hupnum_5_4);

    for(int i = 0; i < cb_next_list.size(); i++){


        cb_hupnum_list[i]->setCurrentIndex(0);
        int node_id = i / 4 + 1;
        if((i % 4) < (node_id - 1)){
            cb_dest_list[i]->setCurrentIndex(i % 4);
            cb_next_list[i]->setCurrentIndex(i % 4);
        }
        else{
            cb_dest_list[i]->setCurrentIndex(i % 4 +1);
            cb_next_list[i]->setCurrentIndex(i % 4 +1);
        }
    }
}

srt_entry_config::~srt_entry_config()
{
    delete ui;
}
