#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>
#include <QDebug>
#include <QTimer>

class ESPReaderApp : public QWidget
{
    Q_OBJECT

public:
    ESPReaderApp(QWidget *parent = nullptr) 
        : QWidget(parent), 
          serialPort(new QSerialPort(this))
    {
        setupUI();
        setupSerial();
    }

private slots:
    void refreshPorts()
    {
        portSelector->clear();
        bool foundValidPort = false;

        const auto ports = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo &portInfo : ports) {
            QString portName = portInfo.portName();
            
            #if defined(Q_OS_LINUX)
            if (portName.startsWith("ttyUSB") || portName.startsWith("ttyACM")) {
                portName = "/dev/" + portName;
                foundValidPort = true;
            }
            #elif defined(Q_OS_WIN)
            foundValidPort = true;
            #elif defined(Q_OS_MACOS)
            if (portName.startsWith("cu.")) {
                portName = "/dev/" + portName;
                foundValidPort = true;
            }
            #endif

            if (foundValidPort) {
                portSelector->addItem(portName, portName);
            }
        }

        if (!foundValidPort) {
            portSelector->addItem("No serial ports found");
            readButton->setEnabled(false);
        }
    }

    void toggleReading()
    {
        if (isReading) {
            stopReading();
        } else {
            startReading();
        }
    }

    void handleReadyRead()
    {
        QByteArray data = serialPort->readAll();
        buffer += QString::fromUtf8(data);

        processBuffer();
    }

    void handleError(QSerialPort::SerialPortError error)
    {
        if (error != QSerialPort::NoError) {
            output->append("Error: " + serialPort->errorString());
            stopReading();
        }
    }

private:
    void setupUI()
    {
        setWindowTitle("ESP32 Data Reader");
        setGeometry(100, 100, 600, 400);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // Port selection
        QHBoxLayout *portLayout = new QHBoxLayout();
        portSelector = new QComboBox(this);
        QPushButton *refreshButton = new QPushButton("Refresh", this);
        portLayout->addWidget(portSelector);
        portLayout->addWidget(refreshButton);

        // Control buttons
        readButton = new QPushButton("Start Reading", this);
        QPushButton *clearButton = new QPushButton("Clear Output", this);

        // Text output
        output = new QTextEdit(this);
        output->setReadOnly(true);

        // Assemble layout
        mainLayout->addLayout(portLayout);
        mainLayout->addWidget(readButton);
        mainLayout->addWidget(clearButton);
        mainLayout->addWidget(output);

        // Connections
        connect(refreshButton, &QPushButton::clicked, this, &ESPReaderApp::refreshPorts);
        connect(readButton, &QPushButton::clicked, this, &ESPReaderApp::toggleReading);
        connect(clearButton, &QPushButton::clicked, output, &QTextEdit::clear);

        refreshPorts();
    }

    void setupSerial()
    {
        connect(serialPort, &QSerialPort::readyRead, this, &ESPReaderApp::handleReadyRead);
        connect(serialPort, &QSerialPort::errorOccurred, this, &ESPReaderApp::handleError);
    }

    void startReading()
    {
        QString portName = portSelector->currentData().toString();
        if (portName.isEmpty()) {
            output->append("Invalid port selected");
            return;
        }

        serialPort->setPortName(portName);
        serialPort->setBaudRate(QSerialPort::Baud115200);
        serialPort->setDataBits(QSerialPort::Data8);
        serialPort->setParity(QSerialPort::NoParity);
        serialPort->setStopBits(QSerialPort::OneStop);
        serialPort->setFlowControl(QSerialPort::NoFlowControl);

        if (serialPort->open(QIODevice::ReadOnly)) {
            isReading = true;
            readButton->setText("Stop Reading");
            output->append("Started reading from " + portName);
            buffer.clear();
        } else {
            output->append("Failed to open port: " + serialPort->errorString());
        }
    }

    void stopReading()
    {
        if (serialPort->isOpen()) {
            serialPort->close();
        }
        isReading = false;
        readButton->setText("Start Reading");
        output->append("Stopped reading");
    }

    void processBuffer()
    {
        int startIndex = buffer.indexOf("START");
        int endIndex = buffer.indexOf("END");

        if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
            QString message = buffer.mid(startIndex + 5, endIndex - startIndex - 5);
            output->append("Received:\n" + message.trimmed());
            buffer.remove(0, endIndex + 3);
        }
    }

    QComboBox *portSelector;
    QPushButton *readButton;
    QTextEdit *output;
    QSerialPort *serialPort;
    
    QString buffer;
    bool isReading = false;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set style
    QApplication::setStyle("Fusion");
    
    ESPReaderApp reader;
    reader.show();
    
    return app.exec();
}

#include "main.moc"

