#include "overlay_55.h"

#include <nitro/mi/memory.h>

#include "global.h"

#include "constants/game_stats.h"
#include "constants/mail.h"

#include "game_stats.h"
#include "player_data.h"

FS_EXTERN_OVERLAY(OVY_56);
FS_EXTERN_OVERLAY(OVY_102);

extern BOOL ov56_021E5C20(OverlayManager *, int *);
extern BOOL ov56_021E5C9C(OverlayManager *, int *);
extern BOOL ov56_021E5CB4(OverlayManager *, int *);

extern BOOL EasyChat_Init(OverlayManager *, int *);
extern BOOL EasyChat_Main(OverlayManager *, int *);
extern BOOL EasyChat_Exit(OverlayManager *, int *);

static const OverlayManagerTemplate ov55_021E5BF4 = {
    .init = ov56_021E5C20,
    .exec = ov56_021E5C9C,
    .exit = ov56_021E5CB4,
    .ovy_id = FS_OVERLAY_ID(OVY_56),
};

static const OverlayManagerTemplate ov55_021E5C04 = {
    .init = EasyChat_Init,
    .exec = EasyChat_Main,
    .exit = EasyChat_Exit,
    .ovy_id = FS_OVERLAY_ID(OVY_102),
};

enum MailAppState {
    MAIL_STATE_OPEN_VIEWER = 0,
    MAIL_STATE_VIEWING,
    MAIL_STATE_EXIT,
    MAIL_STATE_EDIT_SENTENCE_START,
    MAIL_STATE_EDITING,
};

static MailViewerAppArgs *MailViewerAppArgs_New(Mail *mail, enum HeapID heapID);
static void MailViewerAppArgs_Free(MailViewerAppArgs *args);
static void Mail_UpdateFromViewerArgs(Mail *mail, MailViewerAppArgs *args);

static BOOL MailApp_RunSubApplication(OverlayManager **manager) {
    if (*manager != NULL && OverlayManager_Run(*manager)) {
        OverlayManager_Delete(*manager);
        *manager = NULL;
        return TRUE;
    }

    return FALSE;
}

BOOL ov55_UnkApp_Init(OverlayManager *manager, int *state) {
    MailApp *mailApp;
    MailAppArgs *args;

    args = OverlayManager_GetArgs(manager);
    Heap_Create(HEAP_ID_3, HEAP_ID_OV55, 0x1000);
    mailApp = OverlayManager_CreateAndGetData(manager, sizeof(MailApp), HEAP_ID_OV55);
    MI_CpuFill8(mailApp, 0, sizeof(MailApp));

    mailApp->heapID = HEAP_ID_OV55;
    mailApp->viewerArgs = MailViewerAppArgs_New(args->mail, HEAP_ID_OV55);
    mailApp->viewerArgs->options = Save_PlayerData_GetOptionsAddr(args->saveData);
    if (args->writeMode == TRUE) {
        mailApp->viewerArgs->mailType = args->mailType;
    }
    if (mailApp->viewerArgs->mailType >= NUM_MAIL) {
        mailApp->viewerArgs->mailType = MAIL_GRASS;
    }
    mailApp->viewerArgs->writeMode = args->writeMode;
    mailApp->viewerArgs->menuInputStateMgr = args->menuInputStatePtr;
    return TRUE;
}

BOOL ov55_UnkApp_Main(OverlayManager *manager, int *state) {
    MailApp *mailApp = OverlayManager_GetData(manager);
    MailAppArgs *args = OverlayManager_GetArgs(manager);

    switch (*state) {
    case MAIL_STATE_OPEN_VIEWER:
        mailApp->viewerArgs->writeMode = args->writeMode;
        mailApp->subOverlayManager = OverlayManager_New(&ov55_021E5BF4, mailApp->viewerArgs, mailApp->heapID);
        *state = MAIL_STATE_VIEWING;
        break;

    case MAIL_STATE_VIEWING:
        if (!MailApp_RunSubApplication(&mailApp->subOverlayManager)) {
            break;
        }

        switch (mailApp->viewerArgs->result) {
        case 0xFFFF:
            *state = MAIL_STATE_EXIT;
            break;
        case 3:
            *state = MAIL_STATE_EXIT;
            break;
        default:
            *state = MAIL_STATE_EDIT_SENTENCE_START;
            break;
        }

        break;

    case MAIL_STATE_EXIT:
        if (args->writeMode == TRUE) {
            if (mailApp->viewerArgs->result == 3) {
                Mail_UpdateFromViewerArgs(args->mail, mailApp->viewerArgs);
                GameStats_AddScore(Save_GameStats_Get(args->saveData), GAME_STAT_SCORE);
                GameStats_Inc(Save_GameStats_Get(args->saveData), GAME_STAT_UNK46);
                args->mailWritten = TRUE;
            } else {
                args->mailWritten = FALSE;
            }
        }

        return TRUE;

    case MAIL_STATE_EDIT_SENTENCE_START:
        mailApp->easyChatArgs = EasyChat_CreateArgs(2, 0, args->saveData, args->menuInputStatePtr, mailApp->heapID);
        if (MailMsg_IsInit(&mailApp->viewerArgs->sentences[mailApp->viewerArgs->sentenceIndex])) {
            MailMsg_Copy(&mailApp->selectedSentence, &mailApp->viewerArgs->sentences[mailApp->viewerArgs->sentenceIndex]);
        } else {
            MailMsg_Init_WithBank(&mailApp->selectedSentence, MAILMSG_BANK_0293_GMM);
        }
        sub_02090D20(mailApp->easyChatArgs, &mailApp->selectedSentence);
        mailApp->subOverlayManager = OverlayManager_New(&ov55_021E5C04, mailApp->easyChatArgs, mailApp->heapID);
        *state = MAIL_STATE_EDITING;
        break;

    case MAIL_STATE_EDITING:
        if (MailApp_RunSubApplication(&mailApp->subOverlayManager)) {
            if (sub_02090D48(mailApp->easyChatArgs) == 0) {
                sub_02090D60(mailApp->easyChatArgs, &mailApp->viewerArgs->sentences[mailApp->viewerArgs->sentenceIndex]);
            }
            EasyChat_FreeArgs(mailApp->easyChatArgs);
            *state = MAIL_STATE_OPEN_VIEWER;
        }

        break;
    }

    return FALSE;
}

BOOL ov55_UnkApp_Exit(OverlayManager *manager, int *state) {
    MailApp *mailApp = OverlayManager_GetData(manager);
    MailViewerAppArgs_Free(mailApp->viewerArgs);
    OverlayManager_FreeData(manager);
    Heap_Destroy(mailApp->heapID);
    return TRUE;
}

static MailViewerAppArgs *MailViewerAppArgs_New(Mail *mail, enum HeapID heapID) {
    MailViewerAppArgs *args = Heap_Alloc(heapID, sizeof(MailViewerAppArgs));
    MI_CpuFill8(args, 0, sizeof(MailViewerAppArgs));

    args->writeMode = FALSE;
    args->trainerID = Mail_GetOTID(mail);
    args->trainerName = String_New(PLAYER_NAME_LENGTH + 1, heapID);
    CopyU16ArrayToString(args->trainerName, Mail_GetAuthorNamePtr(mail));
    args->mailType = Mail_GetType(mail);
    args->language = Mail_GetLanguage(mail);
    args->gameVersion = Mail_GetVersion(mail);

    for (u16 i = 0; i < 3; i++) {
        args->iconData[i] = sub_0202B404(mail, (u8)i, 2, sub_0202B4E4(mail));
    }

    for (u16 i = 0; i < 3; i++) {
        MailMsg_Copy(&args->sentences[i], Mail_GetUnk20Array(mail, (u8)i));
    }

    return args;
}

static void MailViewerAppArgs_Free(MailViewerAppArgs *args) {
    if (args->trainerName != NULL) {
        String_Delete(args->trainerName);
    }

    Heap_Free(args);
}

static void Mail_UpdateFromViewerArgs(Mail *mail, MailViewerAppArgs *args) {
    for (u16 i = 0; i < 3; i++) {
        Mail_SetMessage(mail, &args->sentences[i], (u8)i);
    }
    Mail_SetType(mail, args->mailType);
}
