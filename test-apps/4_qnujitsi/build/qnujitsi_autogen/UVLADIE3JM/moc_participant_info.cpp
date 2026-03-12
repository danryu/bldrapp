/****************************************************************************
** Meta object code from reading C++ file 'participant_info.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/participant_info.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'participant_info.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSParticipantInfoENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSParticipantInfoENDCLASS = QtMocHelpers::stringData(
    "ParticipantInfo",
    "nameChanged",
    "",
    "audioMutedChanged",
    "videoMutedChanged",
    "isActiveChanged",
    "participantIdChanged",
    "name",
    "audioMuted",
    "videoMuted",
    "isActive",
    "participantId"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSParticipantInfoENDCLASS_t {
    uint offsetsAndSizes[24];
    char stringdata0[16];
    char stringdata1[12];
    char stringdata2[1];
    char stringdata3[18];
    char stringdata4[18];
    char stringdata5[16];
    char stringdata6[21];
    char stringdata7[5];
    char stringdata8[11];
    char stringdata9[11];
    char stringdata10[9];
    char stringdata11[14];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSParticipantInfoENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSParticipantInfoENDCLASS_t qt_meta_stringdata_CLASSParticipantInfoENDCLASS = {
    {
        QT_MOC_LITERAL(0, 15),  // "ParticipantInfo"
        QT_MOC_LITERAL(16, 11),  // "nameChanged"
        QT_MOC_LITERAL(28, 0),  // ""
        QT_MOC_LITERAL(29, 17),  // "audioMutedChanged"
        QT_MOC_LITERAL(47, 17),  // "videoMutedChanged"
        QT_MOC_LITERAL(65, 15),  // "isActiveChanged"
        QT_MOC_LITERAL(81, 20),  // "participantIdChanged"
        QT_MOC_LITERAL(102, 4),  // "name"
        QT_MOC_LITERAL(107, 10),  // "audioMuted"
        QT_MOC_LITERAL(118, 10),  // "videoMuted"
        QT_MOC_LITERAL(129, 8),  // "isActive"
        QT_MOC_LITERAL(138, 13)   // "participantId"
    },
    "ParticipantInfo",
    "nameChanged",
    "",
    "audioMutedChanged",
    "videoMutedChanged",
    "isActiveChanged",
    "participantIdChanged",
    "name",
    "audioMuted",
    "videoMuted",
    "isActive",
    "participantId"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSParticipantInfoENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       5,   49, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x06,    6 /* Public */,
       3,    0,   45,    2, 0x06,    7 /* Public */,
       4,    0,   46,    2, 0x06,    8 /* Public */,
       5,    0,   47,    2, 0x06,    9 /* Public */,
       6,    0,   48,    2, 0x06,   10 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags
       7, QMetaType::QString, 0x00015103, uint(0), 0,
       8, QMetaType::Bool, 0x00015103, uint(1), 0,
       9, QMetaType::Bool, 0x00015103, uint(2), 0,
      10, QMetaType::Bool, 0x00015103, uint(3), 0,
      11, QMetaType::QString, 0x00015103, uint(4), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ParticipantInfo::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSParticipantInfoENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSParticipantInfoENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSParticipantInfoENDCLASS_t,
        // property 'name'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'audioMuted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'videoMuted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isActive'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'participantId'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ParticipantInfo, std::true_type>,
        // method 'nameChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'audioMutedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'videoMutedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isActiveChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'participantIdChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ParticipantInfo::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ParticipantInfo *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->nameChanged(); break;
        case 1: _t->audioMutedChanged(); break;
        case 2: _t->videoMutedChanged(); break;
        case 3: _t->isActiveChanged(); break;
        case 4: _t->participantIdChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ParticipantInfo::*)();
            if (_t _q_method = &ParticipantInfo::nameChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ParticipantInfo::*)();
            if (_t _q_method = &ParticipantInfo::audioMutedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ParticipantInfo::*)();
            if (_t _q_method = &ParticipantInfo::videoMutedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ParticipantInfo::*)();
            if (_t _q_method = &ParticipantInfo::isActiveChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (ParticipantInfo::*)();
            if (_t _q_method = &ParticipantInfo::participantIdChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ParticipantInfo *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->name(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->audioMuted(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->videoMuted(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isActive(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->participantId(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<ParticipantInfo *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setName(*reinterpret_cast< QString*>(_v)); break;
        case 1: _t->setAudioMuted(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setVideoMuted(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setIsActive(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setParticipantId(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
    (void)_a;
}

const QMetaObject *ParticipantInfo::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ParticipantInfo::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSParticipantInfoENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ParticipantInfo::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void ParticipantInfo::nameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ParticipantInfo::audioMutedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ParticipantInfo::videoMutedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ParticipantInfo::isActiveChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void ParticipantInfo::participantIdChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
