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
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnected();
    void onReadyRead();
    void onSendClicked();
    void onSelectFileClicked();
    void onExitClicked();
    void onUserListClicked(QListWidgetItem *item); // 点击用户列表
    void onResetChatTarget(); // 重置为群聊

private:
    QWidget *centralWidget;
    
    // UI
    QLineEdit *ipInput;
    QLineEdit *portInput;
    QLineEdit *nameInput;
    QPushButton *connectBtn;
    QPushButton *exitBtn;

    QTextEdit *chatDisplay;
    QListWidget *userListWidget;
    QLabel *onlineCountLabel;

    // 私聊状态栏
    QLabel *targetLabel; 
    QPushButton *resetTargetBtn;

    QLineEdit *msgInput;
    QPushButton *sendBtn;
    QPushButton *fileBtn;
    QProgressBar *progressBar;

    // Logic
    QTcpSocket *socket;
    QByteArray m_buffer;
    
    QString m_currentTargetName; // 空字符串=群聊

    QFile *m_receivingFile;
    long m_totalBytesReceived;
    long m_fileSizeExpected;
    bool m_isReceivingFile;
};

#endif