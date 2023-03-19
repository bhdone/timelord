#ifndef TL_MSG_IDS_H
#define TL_MSG_IDS_H

#include <string>

// messages send from FrontEnd
enum class TimelordMsgs : int {
    PONG = 1000,
    PROOF = 1010,
    READY = 1020,
    SPEED = 1030,
    CALC_REPLY = 1040,
};

// messages send from Bhd
enum class TimelordClientMsgs : int {
    PING = 2000,
    CALC = 2010,
};

inline std::string MsgIdToString(TimelordMsgs id)
{
    switch (id) {
    case TimelordMsgs::PONG:
        return "(TimelordMsgs)PONG";
    case TimelordMsgs::PROOF:
        return "(TimelordMsgs)PROOF";
    case TimelordMsgs::READY:
        return "(TimelordMsgs)READY";
    case TimelordMsgs::SPEED:
        return "(TimelordMsgs)SPEED";
    case TimelordMsgs::CALC_REPLY:
        return "(TimelordMsgs)CALC_REPLY";
    }
    return "{unknown}";
}

inline std::string MsgIdToString(TimelordClientMsgs id)
{
    switch (id) {
    case TimelordClientMsgs::PING:
        return "(TimelordClientMsgs)PING";
    case TimelordClientMsgs::CALC:
        return "(TimelordClientMsgs)CALC";
    }
    return "{unknown}";
}

#endif
