/*
 * Qt VSP/BRSP socket implementation on Laird and BlueRadios BT chips (Bluetooth LE)
 *
 * Documentation: http://www.lairdtech.com/brandworld/library/Application%20Note%20-%20Using%20VSP%20with%20smartBASIC.pdf
 *
 * Copyright (c) 2016 Matthias Dieter Wallnöfer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Microgate Srl nor the names of its contributors may
 *     be used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "qvspsocket.h"
#include <QMap>
#include <QBuffer>
#include <QVariant>
#include <QAbstractEventDispatcher>

namespace MiVSP
{

using Manufacturer = QVSPSocket::Manufacturer;

// service UUIDs
static const QMap<QBluetoothUuid, Manufacturer> VSP_SERVICE =
{
    {
        QBluetoothUuid(QStringLiteral("569a1101-b87f-490c-92cb-11ba5ea5167c")),
        Manufacturer::Laird
    },
    {
        QBluetoothUuid(QStringLiteral("da2b84f1-6279-48de-bdc0-afbea0226079")),
        Manufacturer::BlueRadios
    }
};

// characteristics
struct Characteristic
{
    QBluetoothUuid MODEM_IN;   // RTS
    QBluetoothUuid MODEM_OUT;  // CTS
    QBluetoothUuid RX_FIFO;    // Client TX
    QBluetoothUuid TX_FIFO;    // Client RX
};
static const QMap<Manufacturer, Characteristic> CHARACTERISTIC =
{
    {
        Manufacturer::Laird,
        {
            QBluetoothUuid(QStringLiteral("569a2003-b87f-490c-92cb-11ba5ea5167c")),
            QBluetoothUuid(QStringLiteral("569a2002-b87f-490c-92cb-11ba5ea5167c")),
            QBluetoothUuid(QStringLiteral("569a2001-b87f-490c-92cb-11ba5ea5167c")),
            QBluetoothUuid(QStringLiteral("569a2000-b87f-490c-92cb-11ba5ea5167c"))
        }
    },
    {
        Manufacturer::BlueRadios,
        {
            QBluetoothUuid(QStringLiteral("0A1934F5-24B8-4F13-9842-37BB167C6AFF")),
            QBluetoothUuid(QStringLiteral("FDD6B4D3-046D-4330-BDEC-1FD0C90CB43B")),
            QBluetoothUuid(QStringLiteral("BF03260C-7205-4C25-AF43-93B1C299D159")),
            QBluetoothUuid(QStringLiteral("18CDA784-4BD3-4370-85BB-BFED91EC86AF"))
        }
    }
};

// only on BlueRadios
static const QBluetoothUuid BRSP_MODE_CHARACTERISTIC(QStringLiteral("A87988B9-694C-479C-900E-95DFA6C00A24"));
static const QByteArray BRSP_MODE_DATA(1, 0x01);

// descriptor
static const QByteArray DESC_NOTIFY_ON = QByteArray::fromHex(QByteArrayLiteral("0100"));
static const QByteArray DESC_NOTIFY_OFF = QByteArray::fromHex(QByteArrayLiteral("0000"));

// RTS/CTS set/unset flags
static const QMap<Manufacturer, QByteArray> MODEM_SET_BIT =
{
    { Manufacturer::Laird, QByteArray(1, 0x01) },
    { Manufacturer::BlueRadios, QByteArray(1, 0x00) }
};
static const QMap<Manufacturer, QByteArray> MODEM_CLEAR_BIT =
{
    { Manufacturer::Laird, QByteArray(1, 0x00) },
    { Manufacturer::BlueRadios, QByteArray(1, 0x01) }
};

// maximum packet data size (20 is the default for Bluetooth LE)
static const int PACKET_SIZE = 20;

/*!
 * \brief VSPSocket::VSPSocket Creates a new Bluetooth LE VSP socket with the
 * default maximum buffer size (4096)
 * \param parent parent
 */
QVSPSocket::QVSPSocket(QObject* parent)
    : QIODevice(parent)
{
}

/*!
 * \brief VSPSocket::VSPSocket Creates a new Bluetooth LE VSP socket with a
 * custom maximum buffer size
 * \param maxBufferSize has to be in the interval of 21 to INT_MAX. 20 byte is the
 * maximum size of 1 Bluetooth LE packet plus the \0 terminator.
 * \param parent parent
 */
QVSPSocket::QVSPSocket(int maxBufferSize, QObject* parent)
    : QIODevice(parent), maxBufferSize(maxBufferSize)
{
}

/*!
 * \brief VSPSocket::~VSPSocket Destroys the Bluetooth LE VSP socket
 *
 * Before it properly closes an open connection by calling close()
 */
QVSPSocket::~QVSPSocket()
{
    if (isOpen())
        close();
}

/*!
 * \brief VSPSocket::writeInternal Writes data to the RX FIFO characteristic
 */
void QVSPSocket::writeInternal()
{
    if (cts)
    {
        QBuffer buff(&writeBuffer);
        buff.open(QIODevice::ReadOnly);
        auto buffer = buff.read(PACKET_SIZE);
        buff.close();
        if (!buffer.isEmpty())
        {
            service->writeCharacteristic(rxFifoChar, buffer);
            emit bytesWritten(buffer.size());
            writeBuffer.remove(0, buffer.size());
        }
    }
}

/*!
 * \brief VSPSocket::connectToDevice Attempts to connect to the VSP service
 * running on the specified device
 * \param remoteDeviceInfo device info returned from the Bluetooth scan
 *
 * The socket first enters ConnectingState and attempts to connect to the device
 * \a remoteDeviceInfo. If a connection is established, VSPSocket enters
 * ConnectedState and emits connected().
 *
 * At any point, the socket can emit error() to signal that an error occurred.
 *
 * Note that most platforms require a pairing prior to connecting to the remote
 * device. Otherwise the connection process may fail.
 *
 * \sa state(), close()
 */
void QVSPSocket::connectToDevice(const QBluetoothDeviceInfo& remoteDeviceInfo)
{
    if (isOpen())
        return;

    controller = QSharedPointer<QLowEnergyController>::create(remoteDeviceInfo);
    connect(controller.data(), static_cast<void(QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error), [this](QLowEnergyController::Error) {
        this->setErrorString(controller->errorString());
        emit error(_error = QLowEnergyService::ServiceError::OperationError);
    });
    connect(controller.data(), &QLowEnergyController::connected, [this]() {
        controller->discoverServices();
    });
    connect(controller.data(), &QLowEnergyController::discoveryFinished, [this]() {
        // look for the first VSP service found
        for (const QBluetoothUuid& uuid: controller->services())
        {
            if (VSP_SERVICE.contains(uuid))
            {
                // this one works, let us enable it
                service = controller->createServiceObject(uuid, controller.data());
                m = VSP_SERVICE[uuid];
                break;
            }
        }
        if (service == nullptr)
        {
            this->setErrorString(tr("No VSP service found"));
            emit error(_error = QLowEnergyService::ServiceError::OperationError);
            return;
        }

        qDebug().noquote() << QStringLiteral("VSP service mode: ") << QVariant::fromValue(m).toString();

        connect(service, static_cast<void(QLowEnergyService::*)(QLowEnergyService::ServiceError)>(&QLowEnergyService::error),
                         [this](QLowEnergyService::ServiceError error) {
            switch (error)
            {
            case QLowEnergyService::ServiceError::OperationError:
                this->setErrorString(tr("Operation error"));
                break;
            case QLowEnergyService::ServiceError::CharacteristicWriteError:
                this->setErrorString(tr("Characteristic write error"));
                break;
            case QLowEnergyService::ServiceError::DescriptorWriteError:
                this->setErrorString(tr("Descriptor write error"));
                break;
            case QLowEnergyService::ServiceError::UnknownError:
                this->setErrorString(tr("Unknown error"));
                break;
            case QLowEnergyService::ServiceError::CharacteristicReadError:
                this->setErrorString(tr("Characteristic read error"));
                break;
            case QLowEnergyService::ServiceError::DescriptorReadError:
                this->setErrorString(tr("Descriptor read error"));
                break;
            default:
                break;
            }
            emit this->error(_error = error);
            return;
        });

        connect(service, &QLowEnergyService::stateChanged, [this](QLowEnergyService::ServiceState newState) {
            if (newState != QLowEnergyService::ServiceDiscovered)
                return; // wait until the service details have been discovered

            rxFifoChar = service->characteristic(CHARACTERISTIC[m].RX_FIFO);
            txFifoChar = service->characteristic(CHARACTERISTIC[m].TX_FIFO);
            modemInChar = service->characteristic(CHARACTERISTIC[m].MODEM_IN);
            modemOutChar = service->characteristic(CHARACTERISTIC[m].MODEM_OUT);
            if (!rxFifoChar.isValid() || !txFifoChar.isValid() || !modemInChar.isValid() || !modemOutChar.isValid())
            {
                this->setErrorString(tr("Cannot retrieve the VSP service characteristics"));
                emit error(_error = QLowEnergyService::ServiceError::OperationError);
                return;
            }

            if (m == Manufacturer::BlueRadios)
            {
                // BlueRadios needs to be changed into data mode first
                brspModeChar = service->characteristic(BRSP_MODE_CHARACTERISTIC);
                if (!brspModeChar.isValid())
                {
                    this->setErrorString(tr("Cannot retrieve the VSP service characteristics"));
                    emit error(_error = QLowEnergyService::ServiceError::OperationError);
                    return;
                }
            }

            txFifoNotify = txFifoChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
            modemOutNotify = modemOutChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
            if (!txFifoNotify.isValid() || !modemOutNotify.isValid())
            {
                this->setErrorString(tr("Cannot detect VSP service notifications"));
                emit error(_error = QLowEnergyService::ServiceError::OperationError);
                return;
            }

            connect(service, &QLowEnergyService::descriptorWritten, [this](const QLowEnergyDescriptor &descriptor, const QByteArray &newValue) {
                if (descriptor == txFifoNotify && newValue == DESC_NOTIFY_ON)
                    service->writeDescriptor(modemOutNotify, DESC_NOTIFY_ON); // enable notify on CTS
                else if (descriptor == modemOutNotify && newValue == DESC_NOTIFY_ON)
                    service->writeCharacteristic(modemInChar, MODEM_SET_BIT[m]); // RTS set
            });

            connect(service, &QLowEnergyService::characteristicChanged, [this](const QLowEnergyCharacteristic &info, const QByteArray &newValue) {
                qDebug() << QByteArrayLiteral("VSP characteristic changed: ") << info.uuid() << QByteArrayLiteral(" new value: ") << newValue;

                if (info == txFifoChar)
                {
                    if (qint64(readBuffer.size()) + newValue.size() + 1 > maxBufferSize)
                    {
                        // there is no space left, should not happen due to data loss
                        service->writeCharacteristic(modemInChar, MODEM_CLEAR_BIT[m]); // RTS clear
                        this->setErrorString(tr("Internal read buffer overflow (max. size %1), data packet dropped").arg(maxBufferSize));
                        emit error(_error = QLowEnergyService::ServiceError::CharacteristicReadError);
                        return;
                    }

                    readBuffer.append(newValue);

                    if (qint64(readBuffer.size()) + PACKET_SIZE + 1 > maxBufferSize)
                        // okay, now the buffer has become full
                        service->writeCharacteristic(modemInChar, MODEM_CLEAR_BIT[m]); // RTS clear

                    if (isOpen())
                        emit readyRead(); // readyRead() emitted only after the handshake completed
                }
                else if (info == modemOutChar)
                {
                    cts = newValue == MODEM_SET_BIT[m];
                    writeInternal(); // CTS set, now write
                }
            });

            connect(service, &QLowEnergyService::characteristicRead, [this](const QLowEnergyCharacteristic &info, const QByteArray &value) {
                qDebug() << QByteArrayLiteral("VSP characteristic read: ") << info.uuid() << QByteArrayLiteral(" value: ") << value;

                if (info == modemOutChar && !isOpen())
                {
                    cts = value == MODEM_SET_BIT[m];

                    // now finally ready to accept
                    QIODevice::open(OpenModeFlag::ReadWrite);

                    emit stateChanged(_state = QBluetoothSocket::SocketState::ConnectedState);
                    emit connected();

                    if (!readBuffer.isEmpty())
                        emit readyRead(); // there might be data left from the handshake
                }
            });

            connect(service, &QLowEnergyService::characteristicWritten, [this](const QLowEnergyCharacteristic &info, const QByteArray &value) {
                qDebug() << QByteArrayLiteral("VSP characteristic written: ") << info.uuid() << QByteArrayLiteral(" value: ") << value;

                if (info == rxFifoChar)
                    writeInternal();
                else if (info == modemInChar)
                {
                    rts = value == MODEM_SET_BIT[m];
                    if (rts && !isOpen())
                        // first RTS written, now read CTS (we could have missed its notification)
                        service->readCharacteristic(modemOutChar);
                }
                else if (info == brspModeChar)
                    // BlueRadios changed into data mode, now proceed as usual
                    service->writeDescriptor(txFifoNotify, DESC_NOTIFY_ON); // enable notify on TX buffer
            });

            // BlueRadios
            if (m == Manufacturer::BlueRadios)
                // BlueRadios needs to be changed into data mode first
                service->writeCharacteristic(brspModeChar, BRSP_MODE_DATA);
            else
                service->writeDescriptor(txFifoNotify, DESC_NOTIFY_ON); // enable notify on TX buffer
        });

        service->discoverDetails();
    });

    controller->connectToDevice();
    emit stateChanged(_state = QBluetoothSocket::SocketState::ConnectingState);
}

/*!
 * \brief VSPSocket::close Closes a connection to a VSP service
 *
 * First the readChannelFinished() signal is emitted and a state change to
 * ClosingState happens.
 * Afterwards all allocated resources are teared down and disconnected() is
 * emitted together with a connection state change to UnconnectedState.
 *
 * At any point, the socket may emit error() to signal that an error occurred.
 *
 * \sa state(), connectToDevice()
 */
void QVSPSocket::close()
{
    if (!isOpen())
        return;

    emit stateChanged(_state = QBluetoothSocket::SocketState::ClosingState);
    emit readChannelFinished();

    controller->disconnectFromDevice();
    QIODevice::close();

    // re-init
    controller.reset();
    service = nullptr;
    cts = false;
    rts = false;
    readBuffer.clear();
    writeBuffer.clear();

    emit stateChanged(_state = QBluetoothSocket::SocketState::UnconnectedState);
    emit disconnected();
}

bool QVSPSocket::isSequential() const
{
    return true; // sockets are always sequential devices
}

qint64 QVSPSocket::bytesAvailable() const
{
    return QIODevice::bytesAvailable() + readBuffer.size();
}

qint64 QVSPSocket::bytesToWrite() const
{
    return writeBuffer.size();
}

bool QVSPSocket::canReadLine() const
{
    return QIODevice::canReadLine() || readBuffer.contains('\n');
}

/*!
 * \brief VSPSocket::state Returns the current state of the socket
 * \return current state
 */
QBluetoothSocket::SocketState QVSPSocket::state() const
{
    return _state;
}

/*!
 * \brief VSPSocket::error Returns the last error
 * \return last error
 */
QLowEnergyService::ServiceError QVSPSocket::error() const
{
    return _error;
}

qint64 QVSPSocket::readData(char *data, qint64 maxlen)
{
    if (!isOpen())
    {
        this->setErrorString(tr("Cannot read while not connected"));
        emit error(_error = QLowEnergyService::ServiceError::OperationError);
        return -1;
    }

    // check for eventual incoming data
    QAbstractEventDispatcher::instance()->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);

    QBuffer buff(&readBuffer);
    buff.open(QIODevice::ReadOnly);
    auto res = buff.read(data, maxlen);
    buff.close();
    readBuffer.remove(0, int(res));

    if (!rts && qint64(readBuffer.size()) + PACKET_SIZE + 1 <= maxBufferSize)
        // buffer flushed, send may continue
        service->writeCharacteristic(modemInChar, MODEM_SET_BIT[m]); // RTS set

    return res;
}

qint64 QVSPSocket::writeData(const char *data, qint64 len)
{
    if (!isOpen())
    {
        this->setErrorString(tr("Cannot write while not connected"));
        emit error(_error = QLowEnergyService::ServiceError::OperationError);
        return -1;
    }

    if (qint64(writeBuffer.size()) + len + 1 > maxBufferSize) {
        this->setErrorString(tr("Internal write buffer overflow (max. size %1), write failed").arg(maxBufferSize));
        emit error(_error = QLowEnergyService::ServiceError::OperationError);
        return -1;
    }

    writeBuffer.append(data, int(len));
    writeInternal(); // try to write immediately, otherwise after CTS is set
    return len;
}

/*!
 * \brief QVSPSocket::unsetRTS Unsets Request to Send (RTS)
 *
 * This manual flow control operation should be called when the application is
 * unable to accept further data (e.g. terminates or goes into standby).
 */
void QVSPSocket::unsetRTS()
{
    if (rts)
        service->writeCharacteristic(modemInChar, MODEM_CLEAR_BIT[m]); // RTS clear
}

/*!
 * \brief QVSPSocket::setRTS Sets Request to Send (RTS)
 *
 * This manual flow control operation should be called when the application is
 * resumed from a previous unset RTS operation (e.g. turns active again).
 *
 * The operation silently fails when the read buffer capacity is exhausted.
 */
void QVSPSocket::setRTS()
{
    if (!rts && qint64(readBuffer.size()) + PACKET_SIZE + 1 <= maxBufferSize)
        // buffer flushed, send may continue
        service->writeCharacteristic(modemInChar, MODEM_SET_BIT[m]); // RTS set
}

} // namespace
