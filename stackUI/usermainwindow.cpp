#include "usermainwindow.h"
#include "ui_usermainwindow.h"
#include <QDebug>
#include "pcommunication.h"
#include <unistd.h>
#include "nodeitem.h"
#include <string>
#include "definefile.h"
#include <sys/stat.h>
#include <QMessageBox>
#include <stdlib.h>
#include "topo.h"
#include <sys/time.h>
#include <algorithm>
#include <QPalette>
#include <QPixmap>


QString python_path("python3 ");
extern int selfNodeID;
int targetNodeID;
int fd;
int bodyIndex=0;
UserMainWindow::UserMainWindow(int self_node_id, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UserMainWindow)
{
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    setWindowFlags(Qt::FramelessWindowHint);
    //setWindowFlags(Qt::Tool|Qt::X11BypassWindowManagerHint|Qt::FramelessWindowHint|Qt::WindowMinimizeButtonHint);//windows窗口不显示图标
    setWindowIcon(QIcon(":/fig/scutxh1"));
    ui->setupUi(this);
    SelfNodeID = self_node_id;

    dataTimer = new QTimer(this);
    headerTimer = new QTimer(this);
    dataTimer->setSingleShot(true);
    headerTimer->setSingleShot(true);
    QRegExp regExp("^[1-9][0-9]{1,8}$"); //^[1-9][0-9]*$ 任意位数正整数

    ui->lineEdit->setValidator(new QRegExpValidator(regExp, this));
    serial = 0;
//    SelfNodeID = selfNodeID;
    m_NodeScrollArea = NULL;
    ib = NULL;
    item = NULL;
    AddNodeMutex = new QMutex();
    tcpSocket = new QTcpSocket(this);
    recvImage = new recv_image(this);
    topo = new Topo(this);
    config_panel_stack_ = new config_panel_stack();
    operating_mode_ = new operating_mode();
    send_img_dwt_fountain_ = new send_img_dwt_fountain();
    send_img_dwt_fountain_->set_ower(this);
    send_img_dwt_fountain_->set_stack_socket(this->getSocket());


    cout<<"SelfNodeID = "<<SelfNodeID<<endl;


    ui->lineEdit->setPlaceholderText("ID of Node");
    ui->pushButton->setText("Login defaul");

    ui->label->setText(QString("Node ***") );
    connect(this,SIGNAL(sendNewNodeID(int)),this,SLOT(AddNewNodeHandler(int)));
    connect(this,SIGNAL(send_recvmessage_to_handler(QString,int)),this,SLOT(RecvMessageHandler(QString,int)));
    connect(ui->pushButton,SIGNAL(clicked(bool)),this, SLOT(on_Pushbutton_clicked()));
    connect(this->tcpSocket, SIGNAL(connected()), this, SLOT(conStatusTure()));
    connect(this->tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(conStatusFalse()));
    connect(this->tcpSocket, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(dataTimer, SIGNAL(timeout()) ,this, SLOT(sendPictureTimer()));
    connect(headerTimer, SIGNAL(timeout()), this, SLOT(sendPictureHeaderTimer()));
    connect(ui->btn_config_panel, SIGNAL(clicked(bool)), this, SLOT(show_config_panel_stack()));
    connect(ui->btn_operating_mode, SIGNAL(clicked(bool)), this, SLOT(show_operating_mode()));
}

UserMainWindow::~UserMainWindow()
{
    delete ui;
}

void UserMainWindow::set_self_nodeid(int node_id){
    SelfNodeID = node_id;
}

void UserMainWindow::transform_user(){
    ui->btn_config_panel->close();
    ui->btn_operating_mode->close();
}

void UserMainWindow::transform_main(){
    ui->lineEdit->close();
    ui->pushButton->close();
    ui->stackedWidget->close();
    ui->label_8->close();
    ui->btn_topo->close();
    this->setFixedSize(280, 140);
    ui->label->setText("Welcome");

    QPixmap pixmap = QPixmap("./fig/ocean.jpg").scaled(this->size());
    ui->label_5->setPixmap(pixmap);
    ui->label_5->setGeometry(0, 0, 280, 140);
}

void UserMainWindow::set_le_node_id(int node_id){
    ui->label->setText(QString("Node%1").arg(node_id));
}

ShowFigWidget* UserMainWindow::get_p_fig_widget(){
    return p_figshow;
}

void UserMainWindow::show_dwt_fountain(){
    send_img_dwt_fountain_->show();
}
void UserMainWindow::show_operating_mode(){
    operating_mode_->show();
}
void UserMainWindow::get_ShowFigWidget(ShowFigWidget *fig_wiget){
    p_figshow = fig_wiget;
}

void UserMainWindow::show_config_panel_stack(){
    config_panel_stack_->show();
}


int UserMainWindow::GetSelfNodeID()
{
//***************** as it said below*****************
    return SelfNodeID;
}

void UserMainWindow::SendSelfNodeID(struct App_DataPackage *p_package_recv)
{
//***************** useless now ************************
    while(1)
    {
        if(SelfNodeID!=0)
        {
            QString qstr = QString("%1").arg(SelfNodeID);
            QByteArray ba = qstr.toLatin1();
            char *p_buffer = ba.data();
            int buffer_size = strlen(p_buffer);
            PCommunication::getInstance()->package_write(p_buffer,buffer_size,3,p_package_recv->SourceID,SelfNodeID);
            qDebug()<<"SelfNodeID send sucessful"<<qstr;
            break;
        }
        else
            sleep(3);
    }
    //free(p_package_recv);
}

void UserMainWindow::addSerial()
{
//************************ serial is the number of a packet send to stack ************
    serial++;
}

uint8_t UserMainWindow::getSerial()
{
//********************** as it said below *********************
    return serial;
}

void UserMainWindow:: connectServer()
{
 // *************************** connect to stack using socket ***********************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

//    弃用全局变量
//    if(!(ui->lineEdit->text().isEmpty())){
//        QString strselfNodeID = ui->lineEdit->text();
//        selfNodeID = strselfNodeID.toInt();
//    }
    cout<<"connect to stack: "<<SelfNodeID<<endl;

    switch(SelfNodeID)
    {
    case 1:
        tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 9080);
        break;
    case 2:
        tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 9081);
        break;
    case 3:
        tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 9082);
        break;
    case 4:
        tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 9083);
        break;
    case 5:
        tcpSocket->connectToHost(QHostAddress("127.0.0.1"), 9084);
        break;
    default:
        QMessageBox::information(this,tr("Warning"),tr("NO SUCH ID IN THE LIST,TRY AGAIN"),QMessageBox::Ok);
        break;
     }
}

void UserMainWindow::sendPixPicture(QString filename, int targetNode, int owerChatwindows){
// ******************send image pix by pix, useless now**************************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QPixmap pix(filename);
    QBuffer pictSaveBuff;
    pictSaveBuff.open(QIODevice::ReadWrite);
    pix.save(&pictSaveBuff, "jpg", 100);
    int pictSizeToHandle = pictSaveBuff.size();
    int pictAlreadHandle = 0;
    struct PicturePackage sendStruct;
    sendStruct.Package_type = 5;
    time_t now;
    time(&now);
    sendStruct.pictureID = now % 255;
    sendStruct.data_size = pictSaveBuff.size();
    int i = 0;
    char sendCharArry[sizeof(PicturePackage)];
    while(pictSizeToHandle > 0){
        if(pictSizeToHandle > sizeof(sendStruct.data_buffer)){
            sendStruct.data_size = sizeof(sendStruct.data_buffer);
            pictSaveBuff.seek(pictAlreadHandle);
            memcpy(sendStruct.data_buffer, pictSaveBuff.data(), sizeof(sendStruct.data_buffer));
        } else {
            sendStruct.data_size = pictSizeToHandle;
            pictSaveBuff.seek(pictAlreadHandle);
            memcpy(sendStruct.data_buffer, pictSaveBuff.data(), pictSizeToHandle);
        }
        sendStruct.serialNum = i++;
        cout<<"data_size : "<<(int)sendStruct.data_size<<"at No."<<sendStruct.serialNum<<endl;
        memcpy(sendCharArry, &sendStruct, sizeof(sendStruct));
        tcpSocket->write(sendCharArry, sizeof(sendCharArry));

        pictAlreadHandle = pictAlreadHandle + sendStruct.data_size;
        pictSizeToHandle = pictSizeToHandle - sendStruct.data_size;

        bzero(sendCharArry, sizeof(sendCharArry));
        bzero(sendStruct.data_buffer, sizeof(sendStruct.data_buffer));
    }
}

QString UserMainWindow::rs_encode_image(QString image_file){
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QString script("./python_script/rs_encode_image.py ");
    script += image_file;
    QString command = python_path + script;
    cout<<command.toStdString().c_str()<<endl;
    system(command.toStdString().c_str());
    return image_file + QString(".rs");

}

void UserMainWindow::sendPicture(QString filename, int targetNode, int owerChatwindows){
//**************************send whole image as a binary file ******************************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    filename = rs_encode_image(filename);
//    filename = filename + QString(".rs");

    cout<<"open file : "<<qstr2str(filename).c_str()<<endl;

    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly)){
        QMessageBox::information(this,tr("Warning"),tr("Open file failed!"),QMessageBox::Ok);
        return;
    }
    cout<<"file size : "<<file.size()<<endl;
    byteToSend = file.size();
    fileSize = file.size();
    QByteArray sendBuff;

    QDataStream send(&sendBuff, QIODevice::ReadOnly);
    send.setVersion(QDataStream::Qt_5_6);

    cout<<"sizeof(ImageData) : "<<sizeof(ImageData)<<"sizeof(ImageHeader) : "<<sizeof(ImageHeader)<<endl;
    cout<<"begin to send picture"<<endl;

    bzero(can, sizeof(can));
    time_t now;
    time(&now);
    bzero(&(sendImageHeader.CRCCheckSum), sizeof(uint32_t));
    sendImageData.Package_type = 8;
    sendImageHeader.Package_type = 7;
    sendImageData.SourceID = sendImageHeader.SourceID = owerChatwindows;
    sendImageData.DestinationID = sendImageHeader.DestinationID = targetNode;
    sendImageData.PictureID = sendImageHeader.PictureID = waitACKImageID = now % 255;

    fileTemp = file.readAll();
    p = (uint8_t*)fileTemp.data();

    bzero(&imageHeaderSize, sizeof(uint32_t));
    for(int i = 0; i < file.size() ; i++){
//        cout<<hex<<*(p + i);
        if(*(p + i) == 255 && *(p + i + 1) == 218){
            ushort SOSLen = (*(p + i + 2)) * 16 + *(p + i + 3);
            imageHeaderSize = i + 4 + SOSLen;
            cout<<"imageHeaderSize = "<<(uint32_t)imageHeaderSize<<endl;
            break;
        }
    }
    dataTimer->start(10);
    cout<<"settimer"<<endl;

}



void UserMainWindow::sendPictureTimer(){
//********************************* send image as binary file, but devide it into header and data before send it ******************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    cout<<"timeout"<<endl;
    if(byteToSend > 0){

        if(offset < imageHeaderSize){
//            cout<<"send header"<<endl;
            sendImageHeader.DataSize = std::min(imageHeaderSize - offset, (uint)PICTUERHEADER_PACKAGE_SIZE);
            sendImageHeader.OffSet = offset;
            cout<<"datasize : "<<(uint)sendImageHeader.DataSize<<" offset : "<<sendImageHeader.OffSet<<endl;
            memcpy(sendImageHeader.data_buffer, p + offset, min(imageHeaderSize - offset, (uint)PICTUERHEADER_PACKAGE_SIZE));
            bzero(can, sizeof(can));
            memcpy(can, &sendImageHeader, sizeof(ImageHeader));
            tcpSocket->write(can, sizeof(can));
            byteToSend = byteToSend - sendImageHeader.DataSize;
            offset = offset + (uint)(sendImageHeader.DataSize);
//            cout<<"offset + sendImageData.DataSize : "<<offset + sendImageData.DataSize<<endl;
            bzero(sendImageHeader.data_buffer, PICTUERHEADER_PACKAGE_SIZE);
//            headerTimer->start(100000);
            int header_wait_time = p_figshow->get_header_wait_time() * 1000;
            headerTimer->start(header_wait_time);

//            测试用，正常使用时要注释掉
//            ImageHeader *p = &sendImageHeader;
//            handleImageHeaderACK(p);
            p_figshow->set_send_image_status(1);
            cout<<"set timer after send header"<<endl;
        } else {
            if(isImageHeaderRecv == true){
                //bodyIndex += 1;
                bodyIndex = ceil(offset/float(PICTUERDATA_PACKAGE_SIZE));
                p_figshow->set_send_image_status(2, bodyIndex);
//            if(isImageHeaderRecv == true || isImageHeaderRecv == false){
//            cout<<"send data"<<endl;
                sendImageData.OffSet = offset;
                sendImageData.DataSize = min((uint)(fileSize - offset), (uint)PICTUERDATA_PACKAGE_SIZE);
                cout<<"datasize : "<<(uint)sendImageData.DataSize<<" offset : "<<sendImageData.OffSet<<endl;
                memcpy(sendImageData.data_buffer, p + offset, min((uint)fileSize - offset, (uint)PICTUERDATA_PACKAGE_SIZE));
                bzero(can, sizeof(can));
                memcpy(can, &sendImageData, sizeof(ImageData));
                tcpSocket->write(can, sizeof(can));
    //            cout<<"send len : "<<sizeof(can)<<"type : "<<sendImageData.Package_type<<endl;
                byteToSend = byteToSend - sendImageData.DataSize;
                offset = offset + (uint)(sendImageData.DataSize);
                bzero(sendImageData.data_buffer, PICTUERDATA_PACKAGE_SIZE);
            } else {
                p_figshow->set_send_image_status(4);
                cout<<"header is not recv !"<<endl;
            }
//        dataTimer->start(20000);
        int packet_interval = p_figshow->get_packet_interval() * 1000;
        dataTimer->start(packet_interval);
        cout<<"set timer after send data"<<endl;
        }
    } else {
        p_figshow->set_send_image_status(0);
        waitACKImageID = false;
        emptyTemp();
    }
}

void UserMainWindow::sendPictureHeaderTimer(){
// ********************* as it said below ******************************
    QMessageBox::information(this,tr("Warning"),tr("Header No Respone, Cancel"),QMessageBox::Ok);
}

void UserMainWindow::disconnectServer()
{
// ********************* as it said below ******************************
    tcpSocket->close();
}

void UserMainWindow::writeData(char* data, int len)
{
// ********************* as it said below ******************************
    tcpSocket->write(data, len);
}

void UserMainWindow::readData()
{
// ****************** read data from stack using socket ************************
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;


    int readPackageNum = (tcpSocket->bytesAvailable())/sizeof(App_DataPackage);
    int byteToHandle = tcpSocket->bytesAvailable();
    char readBuff[tcpSocket->bytesAvailable()];

    tcpSocket->read(readBuff, tcpSocket->bytesAvailable());
    cout<<"data coming to "<<SelfNodeID<<endl;
    int tab = 0;
    int i = 0;
    while(byteToHandle > 0){
//        cout<<"byteToHandle : "<<byteToHandle<<endl;

        switch((int)readBuff[tab]){
            case 5:{
                struct ImagePackage *recvPictBuff = (struct ImagePackage *)malloc(sizeof(ImagePackage));
                bzero(recvPictBuff, sizeof(ImagePackage));
                memcpy(recvPictBuff, readBuff + tab, sizeof(ImagePackage));
                tab = tab + sizeof(ImagePackage);
                byteToHandle -= sizeof(ImagePackage);
                package_handler(recvPictBuff);

                break;
                }
        case 7:{
            struct ImageHeader *recvPictBuff = (struct ImageHeader *)malloc(sizeof(ImageHeader));
            bzero(recvPictBuff, sizeof(ImageHeader));
            memcpy(recvPictBuff, readBuff + tab, sizeof(ImageHeader));
            tab = tab + sizeof(ImageHeader);
            byteToHandle -= sizeof(ImageHeader);
            package_handler(recvPictBuff);
            break;
        }
        case 8:{
            struct ImageData *recvPictBuff = (struct ImageData *)malloc(sizeof(ImageData));
            bzero(recvPictBuff, sizeof(ImageData));
            memcpy(recvPictBuff, readBuff + tab, sizeof(ImageData));
            tab = tab + sizeof(ImageData);
            byteToHandle -= sizeof(ImageData);
            package_handler(recvPictBuff);
            break;
        }
        case 9:{
            struct ImageHeader *recvPictBuff = (struct ImageHeader *)malloc(sizeof(ImageHeader));
            bzero(recvPictBuff, sizeof(ImageHeader));
            memcpy(recvPictBuff, readBuff + tab, sizeof(ImageHeader));
            tab = tab + sizeof(ImageHeader);
            byteToHandle -= sizeof(ImageHeader);
            package_handler(recvPictBuff);
            break;
        }
             default:{
//                cout<<"recv a type not 5 package"<<endl;
                struct App_DataPackage *recvAppBuff = (struct App_DataPackage *)malloc(sizeof(struct App_DataPackage));
                bzero(recvAppBuff, sizeof(recvAppBuff));
                memcpy(recvAppBuff, readBuff + tab, sizeof(App_DataPackage));
                package_handler(recvAppBuff);

//                pthread_t tid_handle;
//                int iRet = pthread_create(&tid_handle, NULL, package_handler, recvAppBuff);
//                if (iRet){
//                     qCritical("handler pthread_create error. iRet=%d, package_type=%d.",iRet,recvAppBuff->Package_type);
//                }
                tab = tab + sizeof(App_DataPackage);
                byteToHandle -= sizeof(App_DataPackage);
                break;
                }
        }
    }

}
recv_image* UserMainWindow::getRecvImage(){
//************** as it said below *****************
    return recvImage;
}

void UserMainWindow::handleImageHeaderACK(ImageHeader *p_package_recv){
//************* receive ack confirm from stack, if not , discard the image data sending  ****************
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;

    cout<<"recv header"<<endl;
    isImageHeaderRecv = true;
    headerTimer->stop();
    int wait_for_send = this->p_figshow->get_header_wait_time();
    dataTimer->start(1000 * wait_for_send);

}

void* UserMainWindow::package_handler(void *args)
{
//******************** handle packet from stack, get more imformation at app_datapackage.h*********************
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;


    uint8_t type ;
    memcpy(&type, args, 1);
    cout<<"recv type "<<(uint)type<<endl;
    if(type == 5 || type == 7 || type == 8 || type == 9){
        struct ImagePackage *p_package_recv = (struct ImagePackage*)args;
        switch(type){
        case 5:
            this->getRecvImage()->recvimage(p_package_recv);
            break;
        case 7:
            this->getRecvImage()->handleImageHeader((ImageHeader*)p_package_recv);
            break;
        case 8:
            this->getRecvImage()->handleImageData((ImageData*)p_package_recv);
            break;
        case 9:
            this->handleImageHeaderACK((ImageHeader*)p_package_recv);
            break;
        }

        free(p_package_recv);
    } else{
        struct App_DataPackage *p_package_recv = (struct App_DataPackage *)args;

        switch(p_package_recv->Package_type)
        {
        case 1:
//            没用到
//            this->DosendDatatoMainWindow(p_package_recv);
            break;
        case 2:
            this->SendSelfNodeID(p_package_recv);
            break;
        case 3:
            this->AddNewNode(p_package_recv->data_buffer);
            this->handleRoutTable(p_package_recv->data_buffer);
            break;
        case 4:
            this->message_recvhandler(p_package_recv);
            break;
//        case 5:
//            this->fig_recvhandler(p_package_recv);
//            break;
        case 6:

            break;
//        default:
//            break;
        }
        free(p_package_recv);

    }
}

void UserMainWindow::conStatusTure()
{
//****************** if connect to stack successful, turn the btn into add node ***********
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;
    this->connectStatus = true;
    cout<<"connect success !"<<endl;
    ui->label->setText("Node " + QString::number(SelfNodeID));
    ui->lineEdit->clear();
    ui->pushButton->setText(tr("Add Node"));
    ui->lineEdit->setPlaceholderText("");
    topo->set_info_window_id(SelfNodeID);

}

void UserMainWindow::conStatusFalse()
{
//******************* if connect to stack fail, turn the btn into Login**************************
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;
    this->connectStatus = false;
    ui->pushButton->setText(tr("Login defaul"));
    ui->lineEdit->setPlaceholderText("ID of Node");
    // 取消弹窗显示
    QMessageBox::information(this,tr("Warning"),tr("Connected Failed! Check the Server"),QMessageBox::Ok);
}

bool UserMainWindow::getConStatus()
{
// ******************** return if UI connect to stack successful *******************
    return connectStatus;
}

void UserMainWindow::on_btn_mini_clicked()
{
// *********************** as it said below ********************
    this->showMinimized();
}

void UserMainWindow::on_btn_close_clicked()
{
// *********************** as it said below ********************
    disconnectServer();
    this->close();
    this->topo->close();
    if(p_figshow != NULL)
        this->p_figshow->close();
    if(recvImage != NULL)
        this->recvImage->close();
    for(ChatWindowsMap::iterator it = m_ChatWindowsMap.begin(); it != m_ChatWindowsMap.end(); it++){
        ChatWindows* p_showwindos = it->second;
        p_showwindos->close();
    }

}

void UserMainWindow::switchBtn()
{
// *********************** as it said below ********************
    if(!getConStatus())
        ui->pushButton->setText(tr("Login defaul"));
    else{
        ui->pushButton->setText(tr("Add Node"));
    }
}

void UserMainWindow::mousePressEvent(QMouseEvent *e)
{
// *********************** as it said below ********************
    position =e->globalPos() -  this->pos();

}

void UserMainWindow::mouseMoveEvent(QMouseEvent *e)
{
// *********************** as it said below ********************
    this->move(e->globalPos() - position);
}

void UserMainWindow::AddNewNode(char *table)
{
// *********************** as it said below ********************
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;

    int tableInt[20], len = 0;
    for(int i = 0 ;i<20 ;++i){
        if((table)[i] == 127)
            break;
        tableInt[i] = (table)[i];
        len++;
    }

    bzero(tableInt, sizeof(tableInt));
    for(int i = 0 ;i < len ;++i){
        tableInt[i] = (table)[i];
        emit sendNewNodeID(tableInt[i]);
    }
}

void UserMainWindow::AddNewNodeHandler(int NewNodeID)
{
// **************** use in AdddNewNode ************************
    AddNodeMutex->lock();
    ChatWindowsMap::iterator it = m_ChatWindowsMap.find(NewNodeID);
    if(it == m_ChatWindowsMap.end())
    {
        targetNodeID = NewNodeID;
        ChatWindows* p_newchatwindows  = new ChatWindows(NewNodeID, this);
//        p_newchatwindows->set_ower(this);

        m_ChatWindowsMap.insert(std::pair<int,ChatWindows*>(NewNodeID,p_newchatwindows));
        ui->stackedWidget->setCurrentIndex(0);
        if(m_NodeScrollArea == NULL)
            m_NodeScrollArea = new QScrollArea();
        if(ib == NULL)
            ib = new ImToolBox;
        if(item == NULL){
//            item = new ImToolItem("已知节点");
            item = new ImToolItem("node list");
        }
        m_NodeScrollArea->setWidgetResizable(true);
        m_NodeScrollArea->setAlignment(Qt::AlignLeft);

        NodeItem *user = new NodeItem(NewNodeID);
        connect(user,SIGNAL(sendshowchatwindowID(int)),this,SLOT(ShowChatWindows(int)));
        if(item->GetIsShowList())
        {
            item->show_or_hide_list();
            item->addItem(user);
            item->show_or_hide_list();
        }
        else
            item->addItem(user);
        ib->addItem(item);
        m_NodeScrollArea->setWidget(ib);
        ui->stackedWidget->setStyleSheet("QWidget{border: 0;}");
        ui->verticalLayout->setContentsMargins(0,0,0,0);
        ui->verticalLayout->setMargin(0);
        ui->verticalLayout->addWidget(m_NodeScrollArea);
        qDebug()<<QString("NewNodeId:%1").arg(NewNodeID);
    }
    AddNodeMutex->unlock();
}
void UserMainWindow::handleRoutTable(char *table)
{
// ********************** hand routTable from stack **********************                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   sleep(1);
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;

    int tableInt[20], len = 0;
    bzero(tableInt, sizeof(tableInt));

    for(int i = 0 ; ;++i){
        if((table)[i] == 127)
            break;
        tableInt[i] = (table)[i];
        len++;
    }

    ChatWindowsMap::iterator it;
    for(int i = 0; tableInt[i] != 127 && i < len; i++){
        it = m_ChatWindowsMap.find(tableInt[i]);
        it->second->onlineTrue();
    }
    ChatWindowsMap::iterator ite;
    for(ite = m_ChatWindowsMap.begin(); ite != m_ChatWindowsMap.end(); ++ite){

        if(ite->second->getIsOnline() == 1){

            emit toChangeOnlineStat(ite->first);
        }
        else if(ite->second->getIsOnline() == 0){

            emit toChangeOfflineStat(ite->first);
        }
    }
    topo->setTopo((uint8_t*)table);
}

void UserMainWindow::ShowChatWindows(int ChatWID)
{
// ****************** as it said *************************
    ChatWindowsMap::iterator it = m_ChatWindowsMap.find(ChatWID);
    if(it != m_ChatWindowsMap.end())
    {
        ChatWindows* p_showwindos = it->second;
        p_showwindos->show();
    }
}

void UserMainWindow::picPix_recvhandler(PicturePackage *p_package_recv){
// *********************** receive image pix by pix ,useless now ******************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    pictRecvTemp.append(p_package_recv->data_buffer, p_package_recv->data_size);
    cout<<"size of temp : "<<pictRecvTemp.size()<<" total : "<<(int)p_package_recv->data_size<<endl;
    QPixmap picture;
    if(p_package_recv->data_size < PICTUER_PACKAGE_SIZE){
        picture.loadFromData(pictRecvTemp);
        cout<<"save picture as "<<"/home/ucc/temp/" + QString::number(p_package_recv->pictureID)<<endl;
        picture.save("/home/ucc/temp/" + QString::number(p_package_recv->pictureID), "jpg", -1);
        pictRecvTemp.resize(0);
    }

}

void UserMainWindow::message_recvhandler(App_DataPackage *p_package_recv)
{
// ***************************** handle message from stack **********************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    int len = p_package_recv->data_size;
    char *p_Buffer = (char *)malloc((len+1)*sizeof(char));
    for(int i = 0; i < len; i++){
        p_Buffer[i] = p_package_recv->data_buffer[i];
        cout<<"recv byte"<<p_Buffer[i]<<endl;
    }
    p_Buffer[len] = 0;

    std::string str = p_Buffer;
    cout<<"len = "<<len<<" recv"<<p_package_recv->data_buffer<<endl;
    QString qstr = str2qstr(str);

    int SourceID = p_package_recv->SourceID;
    emit send_recvmessage_to_handler(qstr,SourceID);
    emit toChangeOnlineStat(SourceID);

}

void UserMainWindow::RecvMessageHandler(QString qstr, int SourceID)
{
// **************************** used in message_recvhandler ********************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    ChatWindowsMap::iterator it = m_ChatWindowsMap.find(SourceID);

    while(it == m_ChatWindowsMap.end())
    {
        AddNewNodeHandler(SourceID);

        it = m_ChatWindowsMap.find(SourceID);
        emit toChangeOnlineStat(SourceID);
    }
    ChatWindows* p_newchatwindows = (ChatWindows*)it->second;
    p_newchatwindows->show();
    p_newchatwindows->show();
    p_newchatwindows->show_recvmessage(qstr);
}

void UserMainWindow::on_Pushbutton_clicked()
{
// *********************** slot of addnode btn **********************
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    if(getConStatus()){
        QString NewNodeID_str = ui->lineEdit->text();
        targetNodeID = NewNodeID_str.toInt();
        if(!NewNodeID_str.isEmpty())
        {
            if(NewNodeID_str.toInt() != SelfNodeID){
            int NewNodeID = NewNodeID_str.toInt();
            AddNewNodeHandler(NewNodeID);
            ShowChatWindows(NewNodeID);
            ui->lineEdit->clear();
            }
            else{
                QMessageBox::information(this,tr("Warning"),tr("Can't Open Yourself, Enter Another Node Id"),QMessageBox::Ok);
            }
        }
        else
        {
            QMessageBox::information(this,tr("Warning"),tr("Please input a ID."),QMessageBox::Ok);
        }
    }
    else{
        connectServer();
    }

}

void UserMainWindow::sendQimagePicture(QString filename, int targetNode, int owerChatwindows){
// ********************* another send image pix by pix , useless now ***************************8
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QImage image;
    image.load(filename, "jpg");
    image.save(filename+"copy.jpg", "JPG");
    cout<<"byteCount : "<<image.byteCount()<<endl;
    cout<<"fromat "<<image.format()<<endl;
    cout<<"depth : "<<image.depth()<<endl;
    uint16_t height = image.height();
    uint16_t width = image.width();
    struct ImagePackage sendImage;
    sendImage.ImageWidth = width;
    sendImage.ImageHeight = height;
    sendImage.Package_type = 5;
    sendImage.SourceID = SelfNodeID;
    sendImage.DestinationID = targetNode;
    time_t now;
    time(&now);
    sendImage.PictureID = now % 255;
    int serialNum = 0;
    unsigned int pixArry[PICTUER_PACKAGE_SIZE / sizeof(unsigned int)];
    char sendCharArry[sizeof(ImagePackage)];
    cout<<"sizeof pix : "<<sizeof(image.pixel(50, 50))<<endl;
    cout<<"total pix : "<<height * width <<endl;
    cout<<"image format :"<<image.format()<<endl;
    cout<<"image has alphaChannel : "<<image.hasAlphaChannel()<<endl;
    for(int i = 0 ; i < height * width ; i++){
        pixArry[i % (PICTUER_PACKAGE_SIZE / sizeof(unsigned int))] = image.pixel(i % width, i / width);
//        cout<<"x : "<<i % width<<" y : "<<i / width<<"pix : "<<pixArry[i % (PICTUER_PACKAGE_SIZE / 4)]<<endl;
        //if buff is full, send it and empty buff
        if(i % (PICTUER_PACKAGE_SIZE / 4) >= (PICTUER_PACKAGE_SIZE / sizeof(int) -1) || i >= height * width - 1 ){
           sendImage.SerialNum = serialNum++;
            memcpy(sendImage.data_buffer, pixArry, sizeof(pixArry));
            memcpy(sendCharArry, &sendImage, sizeof(ImagePackage));
//            cout<<"send type : "<<(int)sendImage.Package_type<<endl;
            int writeLen = tcpSocket->write(sendCharArry, sizeof(ImagePackage));
            cout<<"packet serialNum : "<<sendImage.SerialNum<<" writeLen = "<<writeLen<<endl;
            bzero(sendCharArry, sizeof(sendCharArry));
            bzero(sendImage.data_buffer, sizeof(sendImage.data_buffer));
            bzero(pixArry, sizeof(pixArry));
        }
    }
    cout<<"send finish"<<endl;

}

void  UserMainWindow::picture_recvhandler(PicturePackage *p_package_recv)
{
// ********************** another receive image pix by pix, useless now *********************

    QFile file("/home/ucc/temp/" + QString::number(p_package_recv->pictureID));
    QString qstr = QString::number(p_package_recv->pictureID);
    if(!file.open(QIODevice::ReadWrite)){
        QMessageBox::information(this,tr("Error"),QString("open %1 fail.").arg(qstr),QMessageBox::Ok);
        return;
    }
    if(p_package_recv->serialNum == 0){
        if(!file.resize(p_package_recv->data_size)){
            QMessageBox::information(this,tr("Error"),QString("resize %1 fail.").arg(qstr),QMessageBox::Ok);
            return;
        }
    }
    QByteArray writeBuff;
    writeBuff.append(p_package_recv->data_buffer, p_package_recv->data_size);
    file.seek(p_package_recv->serialNum * PICTUER_PACKAGE_SIZE);
    file.write(writeBuff, p_package_recv->data_size);
}

void UserMainWindow::fig_recvhandler(PicturePackage *p_package_recv)
{
//
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;

    QDateTime time = QDateTime::currentDateTime();//获取系统现在的时间
    QString tstr = time.toString("yyyy-MM-dd_hh:mm:ss_ddd"); //设置显示格式
    QString qstr = QString::number(p_package_recv->pictureID);

    QFile file("/home/ucc/temp/" + QString::number(p_package_recv->pictureID));
    if(!file.open(QIODevice::Append)){
        QMessageBox::information(this,tr("Error"),QString("open %1 fail.").arg(qstr),QMessageBox::Ok);
        return;
    }
    QByteArray writeBuff;
    writeBuff.append(p_package_recv->data_buffer, p_package_recv->data_size);
    cout<<"data_size : "<<(int)p_package_recv->data_size<<" byte No."<<i<<endl;
    cout<<"write "<<writeBuff.size()<<" byte No."<<i++<<endl;
    file.write(writeBuff, p_package_recv->data_size);


}

void UserMainWindow::testSgnal(int sourceID)
{
    QTextStream cin(stdin,  QIODevice::ReadOnly);
    QTextStream cout(stdout,  QIODevice::WriteOnly);
    using namespace std;
    cout<<"recv signal changeOnline   "<<sourceID<<endl;
}

void UserMainWindow::on_btn_topo_clicked()
{
//    Topo* newtopo = new Topo;
    topo->show();

    emit showTopo();
}

QTcpSocket* UserMainWindow::getSocket(){
    return tcpSocket;
}



void UserMainWindow::emptyTemp(){
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    using namespace std;

    cout<<"empty temp"<<endl;
    fileSize = 0;
    fileTemp.resize(0);
    bzero(can, sizeof(can));
    byteToSend = 0;
    p = NULL;
    offset = 0;
    bzero(&sendImageData, sizeof(sendImageData));
    bzero(&sendImageHeader, sizeof(sendImageHeader));

}

send_img_dwt_fountain* UserMainWindow::get_fountain(){
    return send_img_dwt_fountain_;
}
