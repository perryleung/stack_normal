#ifndef CONFIG_PANEL_STACK_H
#define CONFIG_PANEL_STACK_H

#include <QDialog>
#include <QComboBox>
#include "srt_entry_config.h"
#include "uwaloha_resend_config.h"
#include "qpsk_config.h"
#include "fsk_config.h"
#include "fsk_1710_config.h"
#include <QLabel>


namespace Ui {
class config_panel_stack;
}

class config_panel_stack : public QDialog
{
    Q_OBJECT

public:
    explicit config_panel_stack(QWidget *parent = 0);
    ~config_panel_stack();
    void init_();
    QString protocol_config();
    void get_current_roastFish_recipe(QString recipe=QString("./recipe.txt"));

public slots:
    void net_layer_config();
    void mac_layer_config();
    void phy_layer_config();
    void compile_stack();


private:
    Ui::config_panel_stack *ui;
    srt_entry_config *srt_entry_config_;
    uwaloha_resend_config *uwaloha_resend_config_;
    qpsk_config * qpsk_config_;
    fsk_config * fsk_config_;
    fsk_1710_config * fsk_1710_config_;

    std::vector<QComboBox*> cbo_protocol_list;
    std::vector<QLabel*> current_protocol_list;
};

#endif // CONFIG_PANEL_STACK_H
