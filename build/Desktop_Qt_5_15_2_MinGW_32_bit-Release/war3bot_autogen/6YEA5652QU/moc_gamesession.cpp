/****************************************************************************
** Meta object code from reading C++ file 'gamesession.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../include/gamesession.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'gamesession.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_GameSession_t {
    QByteArrayData data[28];
    char stringdata0[291];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_GameSession_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_GameSession_t qt_meta_stringdata_GameSession = {
    {
QT_MOC_LITERAL(0, 0, 11), // "GameSession"
QT_MOC_LITERAL(1, 12, 12), // "sessionEnded"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 9), // "sessionId"
QT_MOC_LITERAL(4, 36, 13), // "dataForwarded"
QT_MOC_LITERAL(5, 50, 4), // "data"
QT_MOC_LITERAL(6, 55, 12), // "QHostAddress"
QT_MOC_LITERAL(7, 68, 4), // "from"
QT_MOC_LITERAL(8, 73, 12), // "playerJoined"
QT_MOC_LITERAL(9, 86, 10), // "PlayerInfo"
QT_MOC_LITERAL(10, 97, 6), // "player"
QT_MOC_LITERAL(11, 104, 10), // "playerLeft"
QT_MOC_LITERAL(12, 115, 7), // "uint8_t"
QT_MOC_LITERAL(13, 123, 8), // "playerId"
QT_MOC_LITERAL(14, 132, 8), // "uint32_t"
QT_MOC_LITERAL(15, 141, 6), // "reason"
QT_MOC_LITERAL(16, 148, 11), // "chatMessage"
QT_MOC_LITERAL(17, 160, 10), // "fromPlayer"
QT_MOC_LITERAL(18, 171, 8), // "toPlayer"
QT_MOC_LITERAL(19, 180, 7), // "message"
QT_MOC_LITERAL(20, 188, 11), // "forwardData"
QT_MOC_LITERAL(21, 200, 4), // "port"
QT_MOC_LITERAL(22, 205, 12), // "sendToClient"
QT_MOC_LITERAL(23, 218, 10), // "clientAddr"
QT_MOC_LITERAL(24, 229, 10), // "clientPort"
QT_MOC_LITERAL(25, 240, 18), // "broadcastToClients"
QT_MOC_LITERAL(26, 259, 17), // "onSocketReadyRead"
QT_MOC_LITERAL(27, 277, 13) // "onPingTimeout"

    },
    "GameSession\0sessionEnded\0\0sessionId\0"
    "dataForwarded\0data\0QHostAddress\0from\0"
    "playerJoined\0PlayerInfo\0player\0"
    "playerLeft\0uint8_t\0playerId\0uint32_t\0"
    "reason\0chatMessage\0fromPlayer\0toPlayer\0"
    "message\0forwardData\0port\0sendToClient\0"
    "clientAddr\0clientPort\0broadcastToClients\0"
    "onSocketReadyRead\0onPingTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_GameSession[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   64,    2, 0x06 /* Public */,
       4,    2,   67,    2, 0x06 /* Public */,
       8,    1,   72,    2, 0x06 /* Public */,
      11,    2,   75,    2, 0x06 /* Public */,
      16,    3,   80,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      20,    3,   87,    2, 0x0a /* Public */,
      22,    3,   94,    2, 0x0a /* Public */,
      25,    1,  101,    2, 0x0a /* Public */,
      26,    0,  104,    2, 0x08 /* Private */,
      27,    0,  105,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 6,    5,    7,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, 0x80000000 | 12, 0x80000000 | 14,   13,   15,
    QMetaType::Void, 0x80000000 | 12, 0x80000000 | 12, QMetaType::QString,   17,   18,   19,

 // slots: parameters
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 6, QMetaType::UShort,    5,    7,   21,
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 6, QMetaType::UShort,    5,   23,   24,
    QMetaType::Void, QMetaType::QByteArray,    5,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void GameSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<GameSession *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sessionEnded((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->dataForwarded((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< const QHostAddress(*)>(_a[2]))); break;
        case 2: _t->playerJoined((*reinterpret_cast< const PlayerInfo(*)>(_a[1]))); break;
        case 3: _t->playerLeft((*reinterpret_cast< uint8_t(*)>(_a[1])),(*reinterpret_cast< uint32_t(*)>(_a[2]))); break;
        case 4: _t->chatMessage((*reinterpret_cast< uint8_t(*)>(_a[1])),(*reinterpret_cast< uint8_t(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3]))); break;
        case 5: _t->forwardData((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< const QHostAddress(*)>(_a[2])),(*reinterpret_cast< quint16(*)>(_a[3]))); break;
        case 6: _t->sendToClient((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< const QHostAddress(*)>(_a[2])),(*reinterpret_cast< quint16(*)>(_a[3]))); break;
        case 7: _t->broadcastToClients((*reinterpret_cast< const QByteArray(*)>(_a[1]))); break;
        case 8: _t->onSocketReadyRead(); break;
        case 9: _t->onPingTimeout(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (GameSession::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&GameSession::sessionEnded)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (GameSession::*)(const QByteArray & , const QHostAddress & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&GameSession::dataForwarded)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (GameSession::*)(const PlayerInfo & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&GameSession::playerJoined)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (GameSession::*)(uint8_t , uint32_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&GameSession::playerLeft)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (GameSession::*)(uint8_t , uint8_t , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&GameSession::chatMessage)) {
                *result = 4;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject GameSession::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_GameSession.data,
    qt_meta_data_GameSession,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *GameSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameSession::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_GameSession.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int GameSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void GameSession::sessionEnded(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void GameSession::dataForwarded(const QByteArray & _t1, const QHostAddress & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void GameSession::playerJoined(const PlayerInfo & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void GameSession::playerLeft(uint8_t _t1, uint32_t _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void GameSession::chatMessage(uint8_t _t1, uint8_t _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
