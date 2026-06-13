#ifndef POKEHEARTGOLD_OVERLAY_55_H
#define POKEHEARTGOLD_OVERLAY_55_H

#include "mail.h"
#include "mail_misc.h"
#include "options.h"
#include "overlay_manager.h"
#include "pm_string.h"

typedef struct MailViewerAppArgs {
    union {
        u16 writeMode;
        u16 result;
    };
    u8 sentenceIndex;
    u8 horizontalSelectionIndex;
    Options *options;
    MenuInputStateMgr *menuInputStateMgr;
    u32 trainerID;
    u8 padding;
    u8 language;
    u8 gameVersion;
    u8 mailType;
    String *trainerName;
    u16 iconData[3];
    MailMessage sentences[3];
} MailViewerAppArgs;

typedef struct MailApp {
    enum HeapID heapID;
    u8 padding[4];
    EasyChatArgs *easyChatArgs;
    OverlayManager *subOverlayManager;
    MailViewerAppArgs *viewerArgs;
    MailMessage selectedSentence;
} MailApp;

BOOL ov55_UnkApp_Init(OverlayManager *manager, int *state);
BOOL ov55_UnkApp_Main(OverlayManager *manager, int *state);
BOOL ov55_UnkApp_Exit(OverlayManager *manager, int *state);

#endif
