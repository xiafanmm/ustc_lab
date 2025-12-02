/*
 * Description: 客户端主窗口头文件，定义UI组件和网络逻辑接口
 * Author: 夏凡
 * Create: 2025-12-02
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTcpSocket>
#include <QProgressBar>
#include <QListWidget>
#include <QFile>
#include <QFileDialog>
#include <QCloseEvent>
#include "../common/Protocol.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void OnConnected();
    void OnReadyRead();
    void OnSendClicked();
    void OnSelectFileClicked();
    void OnExitClicked();
    void OnUserListClicked(QListWidgetItem *item);
    void OnResetChatTarget();

private:
    void InitUi();
    void InitNetwork();
    void HandleLoginMsg(const QByteArray &body);
    void HandleChatMsg(const QByteArray &body);
    void HandlePrivateChatMsg(const QByteArray &body);
    void HandleUserListMsg(const QByteArray &body);
    void HandleFileInfoMsg(const QByteArray &body);
    void HandleFileDataMsg(const QByteArray &body);

    QWidget *centralWidget;
    
    // UI 组件 
    QLineEdit *ipInput;
    QLineEdit *portInput;
    QLineEdit *nameInput;
    QPushButton *connectBtn;
    QPushButton *exitBtn;

    QTextEdit *chatDisplay;
    QListWidget *userListWidget;
    QLabel *onlineCountLabel;

    QLabel *targetLabel; 
    QPushButton *resetTargetBtn;

    QLineEdit *msgInput;
    QPushButton *sendBtn;
    QPushButton *fileBtn;
    QProgressBar *progressBar;

    // 逻辑变量 (小驼峰) 
    QTcpSocket *socket;
    QByteArray recvBuffer;
    
    QString currentTargetName;

    QFile *receivingFile;
    long totalBytesReceived;
    long fileSizeExpected;
    bool isReceivingFile;
};

#endif