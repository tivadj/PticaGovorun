/****************************************************************************
** Meta object code from reading C++ file 'TranscriberViewModel.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.3.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../TranscriberViewModel.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TranscriberViewModel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_TranscriberViewModel_t {
    QByteArrayData data[5];
    char stringdata[67];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_TranscriberViewModel_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_TranscriberViewModel_t qt_meta_stringdata_TranscriberViewModel = {
    {
QT_MOC_LITERAL(0, 0, 20),
QT_MOC_LITERAL(1, 21, 16),
QT_MOC_LITERAL(2, 38, 0),
QT_MOC_LITERAL(3, 39, 7),
QT_MOC_LITERAL(4, 47, 19)
    },
    "TranscriberViewModel\0nextNotification\0"
    "\0message\0audioSamplesChanged"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_TranscriberViewModel[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   24,    2, 0x06 /* Public */,
       4,    0,   27,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,

       0        // eod
};

void TranscriberViewModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        TranscriberViewModel *_t = static_cast<TranscriberViewModel *>(_o);
        switch (_id) {
        case 0: _t->nextNotification((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->audioSamplesChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (TranscriberViewModel::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&TranscriberViewModel::nextNotification)) {
                *result = 0;
            }
        }
        {
            typedef void (TranscriberViewModel::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&TranscriberViewModel::audioSamplesChanged)) {
                *result = 1;
            }
        }
    }
}

const QMetaObject TranscriberViewModel::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_TranscriberViewModel.data,
      qt_meta_data_TranscriberViewModel,  qt_static_metacall, 0, 0}
};


const QMetaObject *TranscriberViewModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TranscriberViewModel::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_TranscriberViewModel.stringdata))
        return static_cast<void*>(const_cast< TranscriberViewModel*>(this));
    return QObject::qt_metacast(_clname);
}

int TranscriberViewModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void TranscriberViewModel::nextNotification(const QString & _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void TranscriberViewModel::audioSamplesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, 0);
}
QT_END_MOC_NAMESPACE
