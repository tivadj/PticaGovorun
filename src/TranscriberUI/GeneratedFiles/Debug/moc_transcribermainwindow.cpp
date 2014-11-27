/****************************************************************************
** Meta object code from reading C++ file 'TranscriberMainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.3.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../TranscriberMainWindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TranscriberMainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_TranscriberMainWindow_t {
    QByteArrayData data[12];
    char stringdata[293];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_TranscriberMainWindow_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_TranscriberMainWindow_t qt_meta_stringdata_TranscriberMainWindow = {
    {
QT_MOC_LITERAL(0, 0, 21),
QT_MOC_LITERAL(1, 22, 22),
QT_MOC_LITERAL(2, 45, 0),
QT_MOC_LITERAL(3, 46, 33),
QT_MOC_LITERAL(4, 80, 7),
QT_MOC_LITERAL(5, 88, 36),
QT_MOC_LITERAL(6, 125, 34),
QT_MOC_LITERAL(7, 160, 32),
QT_MOC_LITERAL(8, 193, 39),
QT_MOC_LITERAL(9, 233, 5),
QT_MOC_LITERAL(10, 239, 40),
QT_MOC_LITERAL(11, 280, 12)
    },
    "TranscriberMainWindow\0pushButtonLoad_Clicked\0"
    "\0transcriberModel_nextNotification\0"
    "message\0transcriberModel_audioSamplesChanged\0"
    "transcriberModel_docOffsetXChanged\0"
    "lineEditFileName_editingFinished\0"
    "horizontalScrollBarSamples_valueChanged\0"
    "value\0transcriberModel_lastMouseDocPosXChanged\0"
    "mouseDocPosX"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_TranscriberMainWindow[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    1,   50,    2, 0x08 /* Private */,
       5,    0,   53,    2, 0x08 /* Private */,
       6,    0,   54,    2, 0x08 /* Private */,
       7,    0,   55,    2, 0x08 /* Private */,
       8,    1,   56,    2, 0x08 /* Private */,
      10,    1,   59,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Float,   11,

       0        // eod
};

void TranscriberMainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        TranscriberMainWindow *_t = static_cast<TranscriberMainWindow *>(_o);
        switch (_id) {
        case 0: _t->pushButtonLoad_Clicked(); break;
        case 1: _t->transcriberModel_nextNotification((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->transcriberModel_audioSamplesChanged(); break;
        case 3: _t->transcriberModel_docOffsetXChanged(); break;
        case 4: _t->lineEditFileName_editingFinished(); break;
        case 5: _t->horizontalScrollBarSamples_valueChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->transcriberModel_lastMouseDocPosXChanged((*reinterpret_cast< float(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject TranscriberMainWindow::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_TranscriberMainWindow.data,
      qt_meta_data_TranscriberMainWindow,  qt_static_metacall, 0, 0}
};


const QMetaObject *TranscriberMainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TranscriberMainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_TranscriberMainWindow.stringdata))
        return static_cast<void*>(const_cast< TranscriberMainWindow*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int TranscriberMainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
