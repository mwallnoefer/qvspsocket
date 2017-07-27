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

#ifndef QVSPSOCKET_GLOBAL_H
#define QVSPSOCKET_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QVSPSOCKET_LIBRARY)
#  define QVSPSOCKETSHARED_EXPORT Q_DECL_EXPORT
#else
#  define QVSPSOCKETSHARED_EXPORT Q_DECL_IMPORT
#endif

#include <QIODevice>
#include <QLowEnergyController>
#include <QBluetoothSocket>
#include <QSharedPointer>

#endif // QVSPSOCKET_GLOBAL_H
