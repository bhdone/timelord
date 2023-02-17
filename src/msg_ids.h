#ifndef TL_MSG_IDS_H
#define TL_MSG_IDS_H

// messages send from FrontEnd
enum class FeMsgs : int
{
    MSGID_FE_READY = 1000,
    MSGID_FE_PROOF = 1010,
    MSGID_FE_STOPPED = 1020,
    MSGID_FE_STATUS = 1030,
};

// messages send from Bhd
enum class BhdMsgs : int
{
    MSGID_BHD_CALC = 2000,
    MSGID_BHD_STOP = 2010,
    MSGID_BHD_QUERY = 2020,
};

#endif
