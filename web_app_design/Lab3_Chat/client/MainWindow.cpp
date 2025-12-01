#include "MainWindow.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QHostAddress>
#include <QFileInfo>
#include <QDir>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Lab3 Ultimate Chat");
    resize(950, 650);

    m_receivingFile = nullptr;
    m_isReceivingFile = false;
    m_currentTargetName = "";

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // --- Top ---
    QVBoxLayout *topLayout = new QVBoxLayout();
    QHBoxLayout *line1 = new QHBoxLayout();
    ipInput = new QLineEdit("127.0.0.1");
    portInput = new QLineEdit("8888"); portInput->setFixedWidth(60);
    nameInput = new QLineEdit(); nameInput->setPlaceholderText("昵称");
    connectBtn = new QPushButton("连接");
    exitBtn = new QPushButton("退出"); exitBtn->setStyleSheet("color:red");

    line1->addWidget(new QLabel("IP:")); line1->addWidget(ipInput);
    line1->addWidget(new QLabel("Port:")); line1->addWidget(portInput);
    line1->addWidget(new QLabel("昵称:")); line1->addWidget(nameInput);
    line1->addWidget(connectBtn);
    line1->addWidget(exitBtn);
    topLayout->addLayout(line1);

    // --- Center ---
    QHBoxLayout *centerLayout = new QHBoxLayout();
    
    // Left
    QVBoxLayout *leftLayout = new QVBoxLayout();
    chatDisplay = new QTextEdit(); chatDisplay->setReadOnly(true);
    
    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLabel = new QLabel("当前模式: 群聊 (所有人)");
    targetLabel->setStyleSheet("font-weight: bold; color: green;");
    resetTargetBtn = new QPushButton("切回群聊");
    resetTargetBtn->setVisible(false);
    targetLayout->addWidget(targetLabel);
    targetLayout->addWidget(resetTargetBtn);
    targetLayout->addStretch();

    progressBar = new QProgressBar(); progressBar->setVisible(false);

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

    // Right
    QVBoxLayout *rightLayout = new QVBoxLayout();
    onlineCountLabel = new QLabel("在线: 0");
    userListWidget = new QListWidget(); userListWidget->setFixedWidth(200);
    rightLayout->addWidget(onlineCountLabel);
    rightLayout->addWidget(new QLabel("双击列表可私聊:"));
    rightLayout->addWidget(userListWidget);

    centerLayout->addLayout(leftLayout);
    centerLayout->addLayout(rightLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(centerLayout);

    sendBtn->setEnabled(false); fileBtn->setEnabled(false);

    // --- Logic ---
    socket = new QTcpSocket(this);

    connect(connectBtn, &QPushButton::clicked, this, [=](){
        if(socket->state() == QAbstractSocket::UnconnectedState) {
            if(nameInput->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Warn", "请输入昵称"); return;
            }
            nameInput->clearFocus(); // 修复输入法焦点问题
            socket->connectToHost(ipInput->text(), portInput->text().toUShort());
        }
    });
    // 回车直接连接
    connect(nameInput, &QLineEdit::returnPressed, connectBtn, &QPushButton::click);
    
    connect(exitBtn, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    connect(userListWidget, &QListWidget::itemClicked, this, &MainWindow::onUserListClicked);
    connect(resetTargetBtn, &QPushButton::clicked, this, &MainWindow::onResetChatTarget);
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(fileBtn, &QPushButton::clicked, this, &MainWindow::onSelectFileClicked);

    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, [=](){
        chatDisplay->append("System: 断开连接");
        sendBtn->setEnabled(false); fileBtn->setEnabled(false);
        connectBtn->setEnabled(true); connectBtn->setText("连接");
        ipInput->setEnabled(true); portInput->setEnabled(true); nameInput->setEnabled(true);
        userListWidget->clear();
        onResetChatTarget();
    });
}

MainWindow::~MainWindow() {
    if(m_receivingFile) delete m_receivingFile;
}

void MainWindow::onUserListClicked(QListWidgetItem *item) {
    QString target = item->text();
    if (target == nameInput->text()) return;
    m_currentTargetName = target;
    targetLabel->setText("当前模式: 私聊 -> " + target);
    targetLabel->setStyleSheet("font-weight: bold; color: blue;");
    resetTargetBtn->setVisible(true);
}

void MainWindow::onResetChatTarget() {
    m_currentTargetName = "";
    targetLabel->setText("当前模式: 群聊 (所有人)");
    targetLabel->setStyleSheet("font-weight: bold; color: green;");
    resetTargetBtn->setVisible(false);
    userListWidget->clearSelection();
}

void MainWindow::onExitClicked() {
    if(socket->state() == QAbstractSocket::ConnectedState) {
        MsgHeader h = {MSG_LOGOUT, 0, 0};
        socket->write((char*)&h, sizeof(h));
        socket->disconnectFromHost();
    }
    close();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    onExitClicked();
    e->accept();
}

void MainWindow::onConnected() {
    chatDisplay->append("System: 连接成功");
    sendBtn->setEnabled(true); fileBtn->setEnabled(true);
    connectBtn->setEnabled(false); connectBtn->setText("已连接");
    ipInput->setEnabled(false); portInput->setEnabled(false); nameInput->setEnabled(false);

    std::string name = nameInput->text().toStdString();
    MsgHeader h = {MSG_LOGIN, (int)name.size(), 0};
    socket->write((char*)&h, sizeof(h));
    socket->write(name.c_str(), name.size());
}

void MainWindow::onSendClicked() {
    QString text = msgInput->text();
    if(text.isEmpty()) return;

    if (m_currentTargetName.isEmpty()) {
        QString fullMsg = "[" + nameInput->text() + "]: " + text;
        std::string content = fullMsg.toStdString();
        MsgHeader h = {MSG_CHAT_TEXT, (int)content.size(), 0};
        socket->write((char*)&h, sizeof(h));
        socket->write(content.c_str(), content.size());
        chatDisplay->append("我: " + text);
    } else {
        QString payload = m_currentTargetName + "|" + text;
        std::string content = payload.toStdString();
        MsgHeader h = {MSG_CHAT_PRIVATE, (int)content.size(), 0};
        socket->write((char*)&h, sizeof(h));
        socket->write(content.c_str(), content.size());
    }
    msgInput->clear();
}

void MainWindow::onReadyRead() {
    m_buffer.append(socket->readAll());
    while (true) {
        if (m_buffer.size() < (int)sizeof(MsgHeader)) break;
        MsgHeader header; memcpy(&header, m_buffer.data(), sizeof(MsgHeader));
        int totalLen = sizeof(MsgHeader) + header.bodyLen;
        if (m_buffer.size() < totalLen) break;

        QByteArray body = m_buffer.mid(sizeof(MsgHeader), header.bodyLen);

        if (header.type == MSG_CHAT_TEXT) {
            chatDisplay->append(QString::fromStdString(std::string(body.data(), body.size())));
        }
        else if (header.type == MSG_CHAT_PRIVATE) {
            QString msg = QString::fromStdString(std::string(body.data(), body.size()));
            chatDisplay->append("<font color=\"blue\">" + msg + "</font>");
        }
        else if (header.type == MSG_USER_LIST) {
            QString listStr = QString::fromStdString(std::string(body.data(), body.size()));
            QStringList names = listStr.split(',');
            userListWidget->clear();
            userListWidget->addItems(names);
            onlineCountLabel->setText("在线: " + QString::number(names.size()));
        }
        else if (header.type == MSG_FILE_INFO) {
            // "FileName|Size" (TargetName 已在服务器剥离)
            QString info = QString::fromStdString(std::string(body.data(), body.size()));
            QStringList parts = info.split('|');
            if (parts.size() >= 2) {
                QString fileName = parts[0]; m_fileSizeExpected = parts[1].toLong();
                QDir d; if (!d.exists("received_files")) d.mkdir("received_files");
                if(m_receivingFile) delete m_receivingFile;
                m_receivingFile = new QFile("received_files/" + fileName);
                if (m_receivingFile->open(QIODevice::WriteOnly)) {
                    m_isReceivingFile = true; m_totalBytesReceived = 0;
                    chatDisplay->append("System: 接收文件 " + fileName);
                    progressBar->setVisible(true); progressBar->setValue(0);
                }
            }
        }
        else if (header.type == MSG_FILE_DATA) {
            if (m_isReceivingFile && m_receivingFile->isOpen()) {
                m_receivingFile->write(body); m_totalBytesReceived += body.size();
                if (m_fileSizeExpected > 0) progressBar->setValue((m_totalBytesReceived*100)/m_fileSizeExpected);
                if (m_totalBytesReceived >= m_fileSizeExpected) {
                    chatDisplay->append("System: 接收完成");
                    m_receivingFile->close(); m_isReceivingFile = false; progressBar->setVisible(false);
                }
            }
        }
        m_buffer.remove(0, totalLen);
    }
}

void MainWindow::onSelectFileClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "文件");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QFileInfo fi(filePath);
    // [格式] TargetName|FileName|Size
    std::string info = m_currentTargetName.toStdString() + "|" + 
                       fi.fileName().toStdString() + "|" + 
                       std::to_string(file.size());
    
    MsgHeader h = {MSG_FILE_INFO, (int)info.size(), 0};
    socket->write((char*)&h, sizeof(h)); socket->write(info.c_str(), info.size());

    if (m_currentTargetName.isEmpty()) chatDisplay->append("System: 群发文件 " + fi.fileName());
    else chatDisplay->append("System: 私发文件 -> " + m_currentTargetName);
    
    progressBar->setVisible(true); progressBar->setValue(0);

    char buf[4096];
    qint64 total = 0;
    while (!file.atEnd()) {
        int len = file.read(buf, sizeof(buf));
        if (len > 0) {
            MsgHeader ch = {MSG_FILE_DATA, len, 0};
            socket->write((char*)&ch, sizeof(ch)); socket->write(buf, len);
            socket->flush(); total+=len;
            progressBar->setValue((total*100)/file.size());
            QCoreApplication::processEvents();
        }
    }
    file.close();
    chatDisplay->append("System: 发送完毕");
    progressBar->setVisible(false);
}