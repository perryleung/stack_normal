#include "operating_mode.h"
#include "ui_operating_mode.h"

operating_mode::operating_mode(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::operating_mode)
{
    ui->setupUi(this);
    setWindowTitle("Select A Mode");
    test_mode_panel_ = new test_mode_panel();
    select_node_ = new select_node();
    connect(ui->btn_test_mode, SIGNAL(clicked(bool)), this, SLOT(show_test_mode_panel()));
    connect(ui->btn_chat_mode, SIGNAL(clicked(bool)), this, SLOT(show_chat_mode()));
}

void operating_mode::show_test_mode_panel(){
    test_mode_panel_->show();
    this->close();
}

void operating_mode::show_chat_mode(){
    select_node_->show();
    this->close();
}

operating_mode::~operating_mode()
{
    delete ui;
}
