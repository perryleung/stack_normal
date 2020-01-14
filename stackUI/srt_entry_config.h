#ifndef SRT_ENTRY_CONFIG_H
#define SRT_ENTRY_CONFIG_H

#include <QDialog>
#include <QComboBox>

namespace Ui {
class srt_entry_config;
}

class srt_entry_config : public QDialog
{
    Q_OBJECT

public:
    explicit srt_entry_config(QWidget *parent = 0);
    ~srt_entry_config();
    void init_();

public slots:
    void set_srt_entrys();
private:
    Ui::srt_entry_config *ui;
    std::vector<QComboBox*> cb_dest_list;
    std::vector<QComboBox*> cb_next_list;
    std::vector<QComboBox*> cb_hupnum_list;
};

#endif // SRT_ENTRY_CONFIG_H
