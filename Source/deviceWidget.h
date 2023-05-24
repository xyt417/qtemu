#ifndef DEVICE_WIDGET
#define DEVICE_WIDGET
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QString>
#include <QTimer>
#include <QMap>
#include <QThread>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QScrollBar>

#include <fstream>

#include <device.h>

const int DISK_BLOCK_SIZE = 4096;  //������̿��С (4096 Byte)
const int DISK_BLOCK_NUM = 20;     //������̿����� (20 Blocks)

#define SUCCESS 0
#define FILE_ERR 1
#define OVERSTEP 2

// �豸��
class DeviceWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DeviceWindow(QString deviceType = "none", QString deviceName = "none", QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Device: " + deviceName);
        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        layout = new QVBoxLayout(centralWidget);

        // ��ʾ����
        printerWidget = new QPlainTextEdit(centralWidget);
        layout->addWidget(printerWidget);
        // ״̬��ǩ
        statusLabel = new QLabel("Status: Free", this);
        statusLabel->setAlignment(Qt::AlignBottom | Qt::AlignRight);
        layout->addWidget(statusLabel);

        layout->setAlignment(statusLabel, Qt::AlignBottom | Qt::AlignRight);
    }

    void setStatus(bool isBusy) {
        statusLabel->setText("Status: " + QString(isBusy ? "Busy" : "Free"));
    }

    void print(const QString& text) {
        printerWidget->insertPlainText(text + '\n');
        printerWidget->moveCursor(QTextCursor::End);
        printerWidget->verticalScrollBar()->setValue(printerWidget->verticalScrollBar()->maximum());
    }

    int writeToFile(int blockIndex, int byteIndex, string text) {
        if(blockIndex < 0 || blockIndex >= DISK_BLOCK_NUM) return OVERSTEP; // ���̿�����Խ��
        if(byteIndex < 0 || byteIndex + text.size() >= DISK_BLOCK_SIZE) return OVERSTEP; // �ֽ�����Խ��
        ofstream outfile("release/Disk/block" + to_string(blockIndex) + ".txt");
        if(!outfile.is_open()) return FILE_ERR; // �ļ���ʧ��
        outfile.seekp(byteIndex, ios::beg); // ��λ����byteIndex���ֽ�  ios::beg: �ļ���ͷ
        outfile << text; // д��text
        outfile.close();
        return SUCCESS;
    }

    int readFromFile(int blockIndex, int byteIndex, int length, string &text) {
        if(blockIndex < 0 || blockIndex >= DISK_BLOCK_NUM) return OVERSTEP; // ���̿�����Խ��
        if(byteIndex < 0 || byteIndex + length >= DISK_BLOCK_SIZE) return OVERSTEP; // �ֽ�����Խ��
        ifstream infile("release/Disk/block" + to_string(blockIndex) + ".txt");
        if(!infile.is_open()) return FILE_ERR; // �ļ���ʧ��
        infile.seekg(byteIndex, ios::beg); // ��λ����byteIndex���ֽ�  ios::beg: �ļ���ͷ
        text.clear(); // ���text
        text.resize(length); // ����text��С
        infile.read(&text[0], length); // ��ȡlength���ֽ�
        infile.close();
        return SUCCESS;
    }    

private:
    QWidget *centralWidget; // ���Ĵ���
    QVBoxLayout *layout; // ���ֹ�����
    QPlainTextEdit *printerWidget; // ��ӡ�����
    QLabel *statusLabel; // ״̬��ǩ

};


// ��������
class DeviceMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DeviceMainWindow(DeviceTable &deviceTable, DeviceQueue &deviceQueue, int logger = 0, QWidget *parent = nullptr)
     : QMainWindow(parent), deviceTable(deviceTable), deviceQueue(deviceQueue), logger(logger) {
        setWindowTitle("DeviceWindow");
        centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        layout = new QVBoxLayout(centralWidget);
        statusLabel = new QLabel(centralWidget);
        layout->addWidget(statusLabel);

        // �����豸����
        createDeviceWindows();
        updateDeviceStatus();

        // ��ʼ���豸ָ��
        occupied_devices = deviceQueue.get_occupied_devices();
        devicePointer = occupied_devices.begin();

        // ���ö�ʱ��
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(running()));
        timer->start(10); // ����Ƶ�� 10ms

    }

    void enableLogger() {logger = 1;}
    void disableLogger() {logger = 0;}

    string argi(string request, int index){ // ����ĵ� i ������
        int i = 1;
        int start = 0;
        int end = request.find(",");
        while(i < index){
            start = request.find(",", start) + 1;
            end = request.find(",", start);
            ++ i;
        }
        return request.substr(start, end - start);
    }

public slots:
    void updateDeviceStatus() {
        // �����豸״̬��ʾ
        QString statusText;
        for (const auto &device : deviceTable.deviceList) {
            statusText += "DeviceName: " + QString::fromStdString(device.name) + "  |  Type: "\
             + QString::fromStdString(device.type) + "  |  DevicePriority: " + QString::number(device.priority)\ 
             + "  |  Status: " + (device.status ? "Busy" : "Free") + "  |  Process: " + QString::fromStdString(device.pname) + "\n";
            if (device.type == "screen") {
                // ������Ļ�豸���ڵ�״̬
                screenWindows[device.name]->setStatus(device.status);
            } else if (device.type == "printer") {
                // ���´�ӡ���豸���ڵ�״̬
                printerWindows[device.name]->setStatus(device.status);
            } else if (device.type == "disk") {
                // ���´����豸���ڵ�״̬
                diskWindows[device.name]->setStatus(device.status);
            }
        }
        statusLabel->setText(statusText);
    }

    void running(){
        // ����occupied_devices
        if(devicePointer == occupied_devices.end()){
            occupied_devices = deviceQueue.get_occupied_devices();
            devicePointer = occupied_devices.begin();
            // ����max_priority
            max_priority = 0;
            for(auto device : occupied_devices){
                int priority = deviceTable[device.first].priority;
                if(priority > max_priority) max_priority = priority;
            }
        }
        // ����occupied_devices
        while(devicePointer != occupied_devices.end() && deviceTable[devicePointer->first].priority < max_priority)
            ++ devicePointer; // ���ȼ�������ߵ��豸��ִ��
        if(devicePointer == occupied_devices.end()) return;
        string device_name = devicePointer->first;
        vector<DevRequest> requests = devicePointer->second;
        string process_name = requests[0].pname;
        string request = requests[0].requestStr;
        // screen
        if(deviceTable[device_name].type == "screen"){
            // request = "print,text"
            // ��Ļ��ӡ
            if(request.find("print") != string::npos){
                if(logger) cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������:[" << request << "]\n";
                screenWindows["screen1"]->print(QString::fromStdString(argi(request, 2)));
            }
        // printer
        }else if(deviceTable[device_name].type == "printer"){
            // request = "print,text"
            // ��ӡ����ӡ
            if(request.find("print") != string::npos){
                if(logger) cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������:[" << request << "]\n";
                printerWindows[device_name]->print(QString::fromStdString(argi(request, 2)));
            }
        // disk
        }else if(deviceTable[device_name].type == "disk"){
            if (request.find("write") != std::string::npos) {
                // д�����
                // request = "write,blockIndex,byteIndex,text"
                int blockIndex = std::stoi(argi(request, 2));
                int byteIndex = std::stoi(argi(request, 3));
                string text = argi(request, 4);

                int flag;
                if ((flag = diskWindows[device_name]->writeToFile(blockIndex, byteIndex, text)) == SUCCESS) {
                    if (logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������:[" << request << "]\n";

                    // �ڴ�������ʾ������Ϣ
                    QString taskInfo = "Write Task - Block: " + QString::number(blockIndex) +
                                    ", Byte: " + QString::number(byteIndex) +
                                    ", Text: " + QString::fromStdString(text);
                    diskWindows[device_name]->print(taskInfo);
                } else if (flag == OVERSTEP) {
                    // ����Խ�����
                    if (logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������Խ��:[" << request << "]\n";

                    // �ڴ�������ʾԽ����Ϣ
                    QString errorInfo = "Error: OverStep - Block: " + QString::number(blockIndex) +
                                        ", Byte: " + QString::number(byteIndex) +
                                        ", Text: " + QString::fromStdString(text);
                    diskWindows[device_name]->print(errorInfo);
                } else if (flag == FILE_ERR) {
                    // �����ļ���ʧ�����
                    if (logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " �������ļ���ʧ��:[" << request << "]\n";

                    // �ڴ�������ʾ�ļ���ʧ����Ϣ
                    QString errorInfo = "Error: File Open Failed - Block: " + QString::number(blockIndex) +
                                        ", Byte: " + QString::number(byteIndex) +
                                        ", Text: " + QString::fromStdString(text);
                    diskWindows[device_name]->print(errorInfo);
                }
            }else if (request.find("read") != std::string::npos) {
                // ��ȡ����
                // request = "read,blockIndex,byteIndex,length,buffernum"
                int blockIndex = std::stoi(argi(request, 2));
                int byteIndex = std::stoi(argi(request, 3));
                int length = std::stoi(argi(request, 4));
                int buffernum = std::stoi(argi(request, 5));
                
                int flag;
                if((flag = diskWindows[device_name]->readFromFile(blockIndex, byteIndex, length, deviceQueue.readInBuffer[buffernum])) == SUCCESS){
                    if(logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������:[" << request << "]\n";

                    // �ڴ�������ʾ������Ϣ
                    QString taskInfo = "Read Task - Block: " + QString::number(blockIndex) +
                                    ", Byte: " + QString::number(byteIndex) +
                                    ", Length: " + QString::number(length) +
                                    ", Content: " + QString::fromStdString(deviceQueue.readInBuffer[buffernum]);
                    diskWindows[device_name]->print(taskInfo);
                }else if(flag == OVERSTEP){
                    // ����Խ�����
                    if(logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " ������Խ��:[" << request << "]\n";

                    // �ڴ�������ʾԽ����Ϣ
                    QString errorInfo = "Error: OverStep - Block: " + QString::number(blockIndex) +
                                        ", Byte: " + QString::number(byteIndex) +
                                        ", Length: " + QString::number(length);
                    diskWindows[device_name]->print(errorInfo);
                }else if(flag == FILE_ERR){
                    // �����ļ���ʧ�����
                    if(logger)
                        std::cout << "�豸 " << device_name << " ִ�н��� " << process_name << " �������ļ���ʧ��:[" << request << "]\n";

                    // �ڴ�������ʾ�ļ���ʧ����Ϣ
                    QString errorInfo = "Error: File Open Failed - Block: " + QString::number(blockIndex) +
                                        ", Byte: " + QString::number(byteIndex) +
                                        ", Length: " + QString::number(length);
                    diskWindows[device_name]->print(errorInfo);
                }

            }
        } // disk

        // �ͷ��豸
        deviceQueue.release_device(device_name, process_name);
        ++ devicePointer;
        updateDeviceStatus(); // �����豸״̬��ʾ
    }    

    void stop() {
        timer->stop();
    }

    void start() {
        timer->start(10);
    }


private:
    void createDeviceWindows() {
        for (const auto &device : deviceTable.deviceList) {
            if (device.type == "screen") {
                // ������Ļ�豸����
                DeviceWindow *screenWindow = new DeviceWindow("screen", QString::fromStdString(device.name), this);
                screenWindows[device.name] = screenWindow;
                screenWindow->show();
            } else if (device.type == "printer") {
                // ������ӡ���豸����
                DeviceWindow *printerWindow = new DeviceWindow("printer", QString::fromStdString(device.name), this);
                printerWindows[device.name] = printerWindow;
                printerWindow->show();
            } else if (device.type == "disk") {
                // ���������豸����
                DeviceWindow *diskWindow = new DeviceWindow("disk", QString::fromStdString(device.name), this);
                diskWindows[device.name] = diskWindow;
                diskWindow->show();
            }
        }
    }

    int logger = 0;

    // running����ʹ��
    map<string, vector<DevRequest>>::iterator devicePointer; // �豸ָ��
    map<string, vector<DevRequest>> occupied_devices; // ��ʱ�豸�ֵ�
    int max_priority = 0; // ������ȼ�

    QWidget *centralWidget;
    QVBoxLayout *layout;
    QLabel *statusLabel;
    QTimer *timer;
    DeviceTable &deviceTable; // �����豸��Ϣ��
    DeviceQueue &deviceQueue; // �����豸����
    QMap<string, DeviceWindow*> screenWindows; // ��Ļ�豸����
    QMap<string, DeviceWindow*> printerWindows; // ��ӡ���豸����
    QMap<string, DeviceWindow*> diskWindows; // �����豸����
};

#endif