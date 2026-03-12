/****************************************************************************
** Meta object code from reading C++ file 'conference_controller.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/conference_controller.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'conference_controller.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSConferenceControllerENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSConferenceControllerENDCLASS = QtMocHelpers::stringData(
    "ConferenceController",
    "connectedChanged",
    "",
    "videoMutedChanged",
    "audioMutedChanged",
    "error",
    "message",
    "onCameraSelectionChanged",
    "connectToConference",
    "QQuickWindow*",
    "rootWindow",
    "host",
    "room",
    "videoWidth",
    "videoHeight",
    "receiveLimit",
    "receiveMaxHeight",
    "disconnectConference",
    "connected",
    "cameraManager",
    "CameraManager*",
    "audioManager",
    "AudioManager*",
    "videoMuted",
    "audioMuted",
    "slot0Info",
    "ParticipantInfo*",
    "slot1Info",
    "slot2Info",
    "slot3Info",
    "slot4Info",
    "slot5Info",
    "slot6Info",
    "slot7Info",
    "slot8Info",
    "slot9Info",
    "slot10Info",
    "slot11Info",
    "slot12Info",
    "slot13Info",
    "slot14Info",
    "slot15Info"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSConferenceControllerENDCLASS_t {
    uint offsetsAndSizes[84];
    char stringdata0[21];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[18];
    char stringdata4[18];
    char stringdata5[6];
    char stringdata6[8];
    char stringdata7[25];
    char stringdata8[20];
    char stringdata9[14];
    char stringdata10[11];
    char stringdata11[5];
    char stringdata12[5];
    char stringdata13[11];
    char stringdata14[12];
    char stringdata15[13];
    char stringdata16[17];
    char stringdata17[21];
    char stringdata18[10];
    char stringdata19[14];
    char stringdata20[15];
    char stringdata21[13];
    char stringdata22[14];
    char stringdata23[11];
    char stringdata24[11];
    char stringdata25[10];
    char stringdata26[17];
    char stringdata27[10];
    char stringdata28[10];
    char stringdata29[10];
    char stringdata30[10];
    char stringdata31[10];
    char stringdata32[10];
    char stringdata33[10];
    char stringdata34[10];
    char stringdata35[10];
    char stringdata36[11];
    char stringdata37[11];
    char stringdata38[11];
    char stringdata39[11];
    char stringdata40[11];
    char stringdata41[11];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSConferenceControllerENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSConferenceControllerENDCLASS_t qt_meta_stringdata_CLASSConferenceControllerENDCLASS = {
    {
        QT_MOC_LITERAL(0, 20),  // "ConferenceController"
        QT_MOC_LITERAL(21, 16),  // "connectedChanged"
        QT_MOC_LITERAL(38, 0),  // ""
        QT_MOC_LITERAL(39, 17),  // "videoMutedChanged"
        QT_MOC_LITERAL(57, 17),  // "audioMutedChanged"
        QT_MOC_LITERAL(75, 5),  // "error"
        QT_MOC_LITERAL(81, 7),  // "message"
        QT_MOC_LITERAL(89, 24),  // "onCameraSelectionChanged"
        QT_MOC_LITERAL(114, 19),  // "connectToConference"
        QT_MOC_LITERAL(134, 13),  // "QQuickWindow*"
        QT_MOC_LITERAL(148, 10),  // "rootWindow"
        QT_MOC_LITERAL(159, 4),  // "host"
        QT_MOC_LITERAL(164, 4),  // "room"
        QT_MOC_LITERAL(169, 10),  // "videoWidth"
        QT_MOC_LITERAL(180, 11),  // "videoHeight"
        QT_MOC_LITERAL(192, 12),  // "receiveLimit"
        QT_MOC_LITERAL(205, 16),  // "receiveMaxHeight"
        QT_MOC_LITERAL(222, 20),  // "disconnectConference"
        QT_MOC_LITERAL(243, 9),  // "connected"
        QT_MOC_LITERAL(253, 13),  // "cameraManager"
        QT_MOC_LITERAL(267, 14),  // "CameraManager*"
        QT_MOC_LITERAL(282, 12),  // "audioManager"
        QT_MOC_LITERAL(295, 13),  // "AudioManager*"
        QT_MOC_LITERAL(309, 10),  // "videoMuted"
        QT_MOC_LITERAL(320, 10),  // "audioMuted"
        QT_MOC_LITERAL(331, 9),  // "slot0Info"
        QT_MOC_LITERAL(341, 16),  // "ParticipantInfo*"
        QT_MOC_LITERAL(358, 9),  // "slot1Info"
        QT_MOC_LITERAL(368, 9),  // "slot2Info"
        QT_MOC_LITERAL(378, 9),  // "slot3Info"
        QT_MOC_LITERAL(388, 9),  // "slot4Info"
        QT_MOC_LITERAL(398, 9),  // "slot5Info"
        QT_MOC_LITERAL(408, 9),  // "slot6Info"
        QT_MOC_LITERAL(418, 9),  // "slot7Info"
        QT_MOC_LITERAL(428, 9),  // "slot8Info"
        QT_MOC_LITERAL(438, 9),  // "slot9Info"
        QT_MOC_LITERAL(448, 10),  // "slot10Info"
        QT_MOC_LITERAL(459, 10),  // "slot11Info"
        QT_MOC_LITERAL(470, 10),  // "slot12Info"
        QT_MOC_LITERAL(481, 10),  // "slot13Info"
        QT_MOC_LITERAL(492, 10),  // "slot14Info"
        QT_MOC_LITERAL(503, 10)   // "slot15Info"
    },
    "ConferenceController",
    "connectedChanged",
    "",
    "videoMutedChanged",
    "audioMutedChanged",
    "error",
    "message",
    "onCameraSelectionChanged",
    "connectToConference",
    "QQuickWindow*",
    "rootWindow",
    "host",
    "room",
    "videoWidth",
    "videoHeight",
    "receiveLimit",
    "receiveMaxHeight",
    "disconnectConference",
    "connected",
    "cameraManager",
    "CameraManager*",
    "audioManager",
    "AudioManager*",
    "videoMuted",
    "audioMuted",
    "slot0Info",
    "ParticipantInfo*",
    "slot1Info",
    "slot2Info",
    "slot3Info",
    "slot4Info",
    "slot5Info",
    "slot6Info",
    "slot7Info",
    "slot8Info",
    "slot9Info",
    "slot10Info",
    "slot11Info",
    "slot12Info",
    "slot13Info",
    "slot14Info",
    "slot15Info"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSConferenceControllerENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
      21,  143, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   80,    2, 0x06,   22 /* Public */,
       3,    0,   81,    2, 0x06,   23 /* Public */,
       4,    0,   82,    2, 0x06,   24 /* Public */,
       5,    1,   83,    2, 0x06,   25 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    0,   86,    2, 0x08,   27 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       8,    7,   87,    2, 0x02,   28 /* Public */,
       8,    6,  102,    2, 0x22,   36 /* Public | MethodCloned */,
       8,    5,  115,    2, 0x22,   43 /* Public | MethodCloned */,
       8,    4,  126,    2, 0x22,   49 /* Public | MethodCloned */,
       8,    3,  135,    2, 0x22,   54 /* Public | MethodCloned */,
      17,    0,  142,    2, 0x02,   58 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    6,

 // slots: parameters
    QMetaType::Void,

 // methods: parameters
    QMetaType::Bool, 0x80000000 | 9, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   10,   11,   12,   13,   14,   15,   16,
    QMetaType::Bool, 0x80000000 | 9, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::Int, QMetaType::Int,   10,   11,   12,   13,   14,   15,
    QMetaType::Bool, 0x80000000 | 9, QMetaType::QString, QMetaType::QString, QMetaType::Int, QMetaType::Int,   10,   11,   12,   13,   14,
    QMetaType::Bool, 0x80000000 | 9, QMetaType::QString, QMetaType::QString, QMetaType::Int,   10,   11,   12,   13,
    QMetaType::Bool, 0x80000000 | 9, QMetaType::QString, QMetaType::QString,   10,   11,   12,
    QMetaType::Void,

 // properties: name, type, flags
      18, QMetaType::Bool, 0x00015001, uint(0), 0,
      19, 0x80000000 | 20, 0x00015409, uint(-1), 0,
      21, 0x80000000 | 22, 0x00015409, uint(-1), 0,
      23, QMetaType::Bool, 0x00015103, uint(1), 0,
      24, QMetaType::Bool, 0x00015103, uint(2), 0,
      25, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      27, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      28, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      29, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      30, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      31, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      32, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      33, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      34, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      35, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      36, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      37, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      38, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      39, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      40, 0x80000000 | 26, 0x00015409, uint(-1), 0,
      41, 0x80000000 | 26, 0x00015409, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ConferenceController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSConferenceControllerENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSConferenceControllerENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSConferenceControllerENDCLASS_t,
        // property 'connected'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'cameraManager'
        QtPrivate::TypeAndForceComplete<CameraManager*, std::true_type>,
        // property 'audioManager'
        QtPrivate::TypeAndForceComplete<AudioManager*, std::true_type>,
        // property 'videoMuted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'audioMuted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'slot0Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot1Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot2Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot3Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot4Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot5Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot6Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot7Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot8Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot9Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot10Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot11Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot12Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot13Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot14Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // property 'slot15Info'
        QtPrivate::TypeAndForceComplete<ParticipantInfo*, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ConferenceController, std::true_type>,
        // method 'connectedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'videoMutedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'audioMutedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'error'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onCameraSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'connectToConference'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QQuickWindow *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'connectToConference'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QQuickWindow *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'connectToConference'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QQuickWindow *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'connectToConference'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QQuickWindow *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'connectToConference'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QQuickWindow *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'disconnectConference'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ConferenceController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ConferenceController *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connectedChanged(); break;
        case 1: _t->videoMutedChanged(); break;
        case 2: _t->audioMutedChanged(); break;
        case 3: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onCameraSelectionChanged(); break;
        case 5: { bool _r = _t->connectToConference((*reinterpret_cast< std::add_pointer_t<QQuickWindow*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[7])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 6: { bool _r = _t->connectToConference((*reinterpret_cast< std::add_pointer_t<QQuickWindow*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[6])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 7: { bool _r = _t->connectToConference((*reinterpret_cast< std::add_pointer_t<QQuickWindow*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[5])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 8: { bool _r = _t->connectToConference((*reinterpret_cast< std::add_pointer_t<QQuickWindow*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->connectToConference((*reinterpret_cast< std::add_pointer_t<QQuickWindow*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->disconnectConference(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ConferenceController::*)();
            if (_t _q_method = &ConferenceController::connectedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ConferenceController::*)();
            if (_t _q_method = &ConferenceController::videoMutedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ConferenceController::*)();
            if (_t _q_method = &ConferenceController::audioMutedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (ConferenceController::*)(const QString & );
            if (_t _q_method = &ConferenceController::error; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 2:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< AudioManager* >(); break;
        case 1:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< CameraManager* >(); break;
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
        case 15:
        case 14:
        case 13:
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7:
        case 6:
        case 5:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< ParticipantInfo* >(); break;
        }
    } else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ConferenceController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isConnected(); break;
        case 1: *reinterpret_cast< CameraManager**>(_v) = _t->cameraManager(); break;
        case 2: *reinterpret_cast< AudioManager**>(_v) = _t->audioManager(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isVideoMuted(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->isAudioMuted(); break;
        case 5: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot0Info(); break;
        case 6: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot1Info(); break;
        case 7: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot2Info(); break;
        case 8: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot3Info(); break;
        case 9: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot4Info(); break;
        case 10: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot5Info(); break;
        case 11: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot6Info(); break;
        case 12: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot7Info(); break;
        case 13: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot8Info(); break;
        case 14: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot9Info(); break;
        case 15: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot10Info(); break;
        case 16: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot11Info(); break;
        case 17: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot12Info(); break;
        case 18: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot13Info(); break;
        case 19: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot14Info(); break;
        case 20: *reinterpret_cast< ParticipantInfo**>(_v) = _t->slot15Info(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<ConferenceController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 3: _t->setVideoMuted(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setAudioMuted(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ConferenceController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConferenceController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSConferenceControllerENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ConferenceController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    return _id;
}

// SIGNAL 0
void ConferenceController::connectedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ConferenceController::videoMutedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ConferenceController::audioMutedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ConferenceController::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
