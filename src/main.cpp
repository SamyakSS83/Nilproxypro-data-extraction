#include <QCoreApplication>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QObject>
#include <QTextStream>
#include <QTimer>
#include <QString>
#include <QDebug>

class SerialReader : public QObject
{
    Q_OBJECT

public:
    SerialReader(QObject *parent = nullptr)
        : QObject(parent), serialPort(new QSerialPort(this))
    {
        // Search for a valid serial port based on OS-specific criteria
        const auto ports = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo &info : ports) {
#if defined(Q_OS_LINUX)
            if (info.portName().startsWith("ttyUSB") || info.portName().startsWith("ttyACM")) {
                m_portName = "/dev/" + info.portName();
                break;
            }
#elif defined(Q_OS_WIN)
            m_portName = info.portName();
            break;
#elif defined(Q_OS_MACOS)
            if (info.portName().startsWith("cu.")) {
                m_portName = "/dev/" + info.portName();
                break;
            }
#endif
        }
        if (m_portName.isEmpty()) {
            QTextStream(stdout) << "No valid serial ports found.\n";
        }
    }

public slots:
    void startReading()
    {
        if (m_portName.isEmpty()) {
            QCoreApplication::quit();
            return;
        }

        serialPort->setPortName(m_portName);
        serialPort->setBaudRate(QSerialPort::Baud115200);
        serialPort->setDataBits(QSerialPort::Data8);
        serialPort->setParity(QSerialPort::NoParity);
        serialPort->setStopBits(QSerialPort::OneStop);
        serialPort->setFlowControl(QSerialPort::NoFlowControl);

        if (serialPort->open(QIODevice::ReadOnly)) {
            QTextStream(stdout) << "Started reading from " << m_portName << "\n";
            connect(serialPort, &QSerialPort::readyRead, this, &SerialReader::handleReadyRead);
        } else {
            QTextStream(stdout) << "Failed to open port " << m_portName << ": " 
                                  << serialPort->errorString() << "\n";
            QCoreApplication::quit();
        }
    }

    void handleReadyRead()
    {
        QByteArray data = serialPort->readAll();
        QString text = QString::fromUtf8(data);
        QTextStream(stdout) << "Received: " << text.trimmed() << "\n";
    }

private:
    QSerialPort *serialPort;
    QString m_portName;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    SerialReader reader;
    // Use a single shot timer to start reading after the app event loop starts
    QTimer::singleShot(0, &reader, SLOT(startReading()));

    return app.exec();
}

#include "main.moc"