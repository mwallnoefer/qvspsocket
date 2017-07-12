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

#ifndef QVSPSOCKET_H
#define QVSPSOCKET_H

#include "qvspsocket_global.h"

namespace MiVSP {

enum class Manufacturer
{
    Laird,
    BlueRadios
};

class QVSPSOCKETSHARED_EXPORT QVSPSocket : public QIODevice
{
    Q_OBJECT

    QBluetoothSocket::SocketState _state = QBluetoothSocket::SocketState::UnconnectedState;
    QLowEnergyService::ServiceError _error = QLowEnergyService::ServiceError::NoError;

    Manufacturer m = Manufacturer::Laird;

    QPointer<QLowEnergyController> controller;
    QPointer<QLowEnergyService> service;

    QLowEnergyCharacteristic rxFifoChar;
    QLowEnergyCharacteristic txFifoChar;
    QLowEnergyCharacteristic modemInChar;
    QLowEnergyCharacteristic modemOutChar;
    QLowEnergyCharacteristic brspModeChar;

    QLowEnergyDescriptor txFifoNotify;
    QLowEnergyDescriptor modemOutNotify;

    bool cts = false; // CTS = clear to send to device (set by device)
    bool rts = false; // RTS = request to send from device (set by us)

    int maxBufferSize = 4096; // maximum input and output buffer size 21 .. INT_MAX
    QByteArray readBuffer;
    QByteArray writeBuffer;

    void writeInternal();

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

public:
    explicit QVSPSocket(QObject* parent = nullptr);
    explicit QVSPSocket(int maxBufferSize, QObject* parent = nullptr);
    virtual ~QVSPSocket();

    void connectToDevice(const QBluetoothDeviceInfo& remoteDeviceInfo);
    void disconnectFromService(); // synonyme for close()
    void close() override;

    bool isSequential() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;
    bool canReadLine() const override;

    void unsetRTS();
    void setRTS();

    QBluetoothSocket::SocketState state() const;
    QLowEnergyService::ServiceError error() const;

signals:
    void connected();
    void disconnected();
    void stateChanged(QBluetoothSocket::SocketState state);
    void error(QLowEnergyService::ServiceError error);
};

/*!
 * \brief QVSPSocket::disconnectFromService Closes a connection to a VSP service
 *
 * This function is a synonyme to close() and is provided for compatibility with
 * QBluetoothSocket.
 *
 * \sa close()
 */
inline void QVSPSocket::disconnectFromService() { close(); }

} // namespace

#endif // QVSPSOCKET_H
