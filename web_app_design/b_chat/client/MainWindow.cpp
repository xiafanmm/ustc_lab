/*
 * Description: 客户端主窗口实现，包含UI构建与消息处理逻辑
 * Author: 夏凡
 * Create: 2025-12-02
 */

#include "MainWindow.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QHostAddress>
#include <QFileInfo>
#include <QDir>

// 初始化UI布局 [cite: 389]
void MainWindow::InitUi()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 顶部配置栏
    QVBoxLayout *topLayout = new QVBoxLayout();
    QHBoxLayout *line1 = new QHBoxLayout();
    ipInput = new QLineEdit("127.0.0.1");
    portInput = new QLineEdit("8888");
    portInput->setFixedWidth(60);
    nameInput = new QLineEdit();
    nameInput->setPlaceholderText("昵称");
    connectBtn = new QPushButton("连接");
    exitBtn = new QPushButton("退出");
    exitBtn->setStyleSheet("color:red");

    line1->addWidget(new QLabel("IP:"));
    line1->addWidget(ipInput);
    line1->addWidget(new QLabel("Port:"));
    line1->addWidget(portInput);
    line1->addWidget(new QLabel("昵称:"));
    line1->addWidget(nameInput);
    line1->addWidget(connectBtn);
    line1->addWidget(exitBtn);
    topLayout->addLayout(line1);

    // 中间区域
    QHBoxLayout *centerLayout = new QHBoxLayout();
    
    // 左侧聊天区
    QVBoxLayout *leftLayout = new QVBoxLayout();
    chatDisplay = new QTextEdit();
    chatDisplay->setReadOnly(true);
    
    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLabel = new QLabel("当前模式: 群聊 (所有人)");
    targetLabel->setStyleSheet("font-weight: bold; color: green;");
    resetTargetBtn = new QPushButton("切回群聊");
    resetTargetBtn->setVisible(false);
    targetLayout->addWidget(targetLabel);
    targetLayout->addWidget(resetTargetBtn);
    targetLayout->addStretch();

    progressBar = new QProgressBar();
    progressBar->setVisible(false);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    msgInput = new QLineEdit();
    sendBtn = new QPushButton("发送");
    fileBtn = new QPushButton("传文件");
    inputLayout->addWidget(msgInput);
    inputLayout->addWidget(sendBtn);
    inputLayout->addWidget(fileBtn);

    leftLayout->addWidget(chatDisplay);
    leftLayout->addLayout(targetLayout);
    leftLayout->addWidget(progressBar);
    leftLayout->addLayout(inputLayout);

    // 右侧用户列表
    QVBoxLayout *rightLayout = new QVBoxLayout();
    onlineCountLabel = new QLabel("在线: 0");
    userListWidget = new QListWidget();
    userListWidget->setFixedWidth(200);
    rightLayout->addWidget(onlineCountLabel);
    rightLayout->addWidget(new QLabel("双击列表可私聊:"));
    rightLayout->addWidget(userListWidget);

    centerLayout->addLayout(leftLayout);
    centerLayout->addLayout(rightLayout);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(centerLayout);

    sendBtn->setEnabled(false);
    fileBtn->setEnabled(false);
}

// 初始化网络连接信号槽 [cite: 389]
void MainWindow::InitNetwork()
{
    socket = new QTcpSocket(this);

    connect(connectBtn, &QPushButton::clicked, this, [=]() {
        if (socket->state() == QAbstractSocket::UnconnectedState) {
            if (nameInput->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Warn", "请输入昵称");
                return;
            }
            nameInput->clearFocus();
            socket->connectToHost(ipInput->text(), portInput->text().toUShort());
        }
    });

    connect(nameInput, &QLineEdit::returnPressed, connectBtn, &QPushButton::click);
    connect(exitBtn, &QPushButton::clicked, this, &MainWindow::OnExitClicked);
    connect(userListWidget, &QListWidget::itemClicked, this, &MainWindow::OnUserListClicked);
    connect(resetTargetBtn, &QPushButton::clicked, this, &MainWindow::OnResetChatTarget);
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::OnSendClicked);
    connect(fileBtn, &QPushButton::clicked, this, &MainWindow::OnSelectFileClicked);

    connect(socket, &QTcpSocket::connected, this, &MainWindow::OnConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::OnReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [=]() {
        chatDisplay->append("System: 断开连接");
        sendBtn->setEnabled(false);
        fileBtn->setEnabled(false);
        connectBtn->setEnabled(true);
        connectBtn->setText("连接");
        ipInput->setEnabled(true);
        portInput->setEnabled(true);
        nameInput->setEnabled(true);
        userListWidget->clear();
        OnResetChatTarget();
    });
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Lab3 Ultimate Chat");
    resize(950, 650);

    receivingFile = nullptr;
    isReceivingFile = false;
    currentTargetName = "";

    InitUi();
    InitNetwork();
}

MainWindow::~MainWindow()
{
    if (receivingFile != nullptr) {
        delete receivingFile;
    }
}

void MainWindow::OnUserListClicked(QListWidgetItem *item)
{
    QString target = item->text();
    if (target == nameInput->text()) {
        return;
    }
    currentTargetName = target;
    targetLabel->setText("当前模式: 私聊 -> " + target);
    targetLabel->setStyleSheet("font-weight: bold; color: blue;");
    resetTargetBtn->setVisible(true);
}

void MainWindow::OnResetChatTarget()
{
    currentTargetName = "";
    targetLabel->setText("当前模式: 群聊 (所有人)");
    targetLabel->setStyleSheet("font-weight: bold; color: green;");
    resetTargetBtn->setVisible(false);
    userListWidget->clearSelection();
}

void MainWindow::OnExitClicked()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        MsgHeader h = {MSG_LOGOUT, 0, 0};
        socket->write((char *)&h, sizeof(h));
        socket->disconnectFromHost();
    }
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    OnExitClicked();
    event->accept();
}

void MainWindow::OnConnected()
{
    chatDisplay->append("System: 连接成功");
    sendBtn->setEnabled(true);
    fileBtn->setEnabled(true);
    connectBtn->setEnabled(false);
    connectBtn->setText("已连接");
    ipInput->setEnabled(false);
    portInput->setEnabled(false);
    nameInput->setEnabled(false);

    std::string name = nameInput->text().toStdString();
    MsgHeader h = {MSG_LOGIN, (int)name.size(), 0};
    socket->write((char *)&h, sizeof(h));
    socket->write(name.c_str(), name.size());
}

void MainWindow::OnSendClicked()
{
    QString text = msgInput->text();
    if (text.isEmpty()) {
        return;
    }

    if (currentTargetName.isEmpty()) {
        QString fullMsg = "[" + nameInput->text() + "]: " + text;
        std::string content = fullMsg.toStdString();
        MsgHeader h = {MSG_CHAT_TEXT, (int)content.size(), 0};
        socket->write((char *)&h, sizeof(h));
        socket->write(content.c_str(), content.size());
        chatDisplay->append("我: " + text);
    } else {
        QString payload = currentTargetName + "|" + text;
        std::string content = payload.toStdString();
        MsgHeader h = {MSG_CHAT_PRIVATE, (int)content.size(), 0};
        socket->write((char *)&h, sizeof(h));
        socket->write(content.c_str(), content.size());
    }
    msgInput->clear();
}

void MainWindow::HandleChatMsg(const QByteArray &body)
{
    chatDisplay->append(QString::fromStdString(std::string(body.data(), body.size())));
}

void MainWindow::HandlePrivateChatMsg(const QByteArray &body)
{
    QString msg = QString::fromStdString(std::string(body.data(), body.size()));
    chatDisplay->append("<font color=\"blue\">" + msg + "</font>");
}

void MainWindow::HandleUserListMsg(const QByteArray &body)
{
    QString listStr = QString::fromStdString(std::string(body.data(), body.size()));
    QStringList names = listStr.split(',');
    userListWidget->clear();
    userListWidget->addItems(names);
    onlineCountLabel->setText("在线: " + QString::number(names.size()));
}

void MainWindow::HandleFileInfoMsg(const QByteArray &body)
{
    QString info = QString::fromStdString(std::string(body.data(), body.size()));
    QStringList parts = info.split('|');
    if (parts.size() >= 2) {
        QString fileName = parts[0];
        fileSizeExpected = parts[1].toLong();
        QDir d;
        if (!d.exists("received_files")) {
            d.mkdir("received_files");
        }
        if (receivingFile != nullptr) {
            delete receivingFile;
        }
        receivingFile = new QFile("received_files/" + fileName);
        if (receivingFile->open(QIODevice::WriteOnly)) {
            isReceivingFile = true;
            totalBytesReceived = 0;
            chatDisplay->append("System: 接收文件 " + fileName);
            progressBar->setVisible(true);
            progressBar->setValue(0);
        }
    }
}

void MainWindow::HandleFileDataMsg(const QByteArray &body)
{
    if (isReceivingFile && receivingFile->isOpen()) {
        receivingFile->write(body);
        totalBytesReceived += body.size();
        if (fileSizeExpected > 0) {
            progressBar->setValue((totalBytesReceived * 100) / fileSizeExpected);
        }
        if (totalBytesReceived >= fileSizeExpected) {
            chatDisplay->append("System: 接收完成");
            receivingFile->close();
            isReceivingFile = false;
            progressBar->setVisible(false);
        }
    }
}

void MainWindow::OnReadyRead()
{
    recvBuffer.append(socket->readAll());
    while (true) {
        if (recvBuffer.size() < (int)sizeof(MsgHeader)) {
            break;
        }
        MsgHeader header;
        memcpy(&header, recvBuffer.data(), sizeof(MsgHeader));
        int totalLen = sizeof(MsgHeader) + header.bodyLen;
        if (recvBuffer.size() < totalLen) {
            break;
        }

        QByteArray body = recvBuffer.mid(sizeof(MsgHeader), header.bodyLen);

        if (header.type == MSG_CHAT_TEXT) {
            HandleChatMsg(body);
        } else if (header.type == MSG_CHAT_PRIVATE) {
            HandlePrivateChatMsg(body);
        } else if (header.type == MSG_USER_LIST) {
            HandleUserListMsg(body);
        } else if (header.type == MSG_FILE_INFO) {
            HandleFileInfoMsg(body);
        } else if (header.type == MSG_FILE_DATA) {
            HandleFileDataMsg(body);
        }
        recvBuffer.remove(0, totalLen);
    }
}

void MainWindow::OnSelectFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "文件");
    if (filePath.isEmpty()) {
        return;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QFileInfo fi(filePath);
    std::string info = currentTargetName.toStdString() + "|" + 
                       fi.fileName().toStdString() + "|" + 
                       std::to_string(file.size());
    
    MsgHeader h = {MSG_FILE_INFO, (int)info.size(), 0};
    socket->write((char *)&h, sizeof(h));
    socket->write(info.c_str(), info.size());

    if (currentTargetName.isEmpty()) {
        chatDisplay->append("System: 群发文件 " + fi.fileName());
    } else {
        chatDisplay->append("System: 私发文件 -> " + currentTargetName);
    }
    
    progressBar->setVisible(true);
    progressBar->setValue(0);

    char buf[4096];
    qint64 total = 0;
    while (!file.atEnd()) {
        int len = file.read(buf, sizeof(buf));
        if (len > 0) {
            MsgHeader ch = {MSG_FILE_DATA, len, 0};
            socket->write((char *)&ch, sizeof(ch));
            socket->write(buf, len);
            socket->flush();
            total += len;
            progressBar->setValue((total * 100) / file.size());
            QCoreApplication::processEvents();
        }
    }
    file.close();
    chatDisplay->append("System: 发送完毕");
    progressBar->setVisible(false);
}