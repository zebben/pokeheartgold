#include "overlay_54.h"

#include <nitro/spi/ARM9/pm.h>

#include "global.h"

#include "data/resdat.naix"
#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0045.h"

#include "bg_window.h"
#include "font.h"
#include "gf_gfx_loader.h"
#include "menu_input_state.h"
#include "msgdata.h"
#include "options.h"
#include "render_text.h"
#include "render_window.h"
#include "sound.h"
#include "sprite.h"
#include "sprite_system.h"
#include "system.h"
#include "touchscreen.h"
#include "unk_02005D10.h"
#include "unk_0200FA24.h"
#include "unk_0203A3B0.h"
#include "vram_transfer_manager.h"

// Not to be confused with `Options`, which is almost exactly the same, save for two members being swapped. SMH
typedef struct OptionsMenu {
    u16 textSpeed : 4;
    u16 soundMode : 2;
    u16 battleScene : 1;
    u16 battleStyle : 1;
    u16 buttonMode : 2;
    u16 messageBoxStyle : 5;
} OptionsMenu;

typedef struct OptionsMenuEntry {
    u16 numChoices;
    u16 selected;
    String *choices[20];
} OptionsMenuEntry;

enum OptionsMenuEntryID {
    ENTRY_TEXT_SPEED = 0,
    ENTRY_BATTLE_SCENE,
    ENTRY_BATTLE_STYLE,
    ENTRY_SOUND_MODE,
    ENTRY_BUTTON_MODE,
    ENTRY_MESSAGE_BOX_FRAME,
    ENTRY_CLOSE,

    MAX_ENTRIES,
};

typedef struct OptionsMenuData {
    enum HeapID heapID;
    u32 state;
    u32 subState;
    u32 dummy0C; // unused, game writes 0 here when it's about to start a fade, but never reads from here
    u32 saveSelections : 2;
    u32 cursor : 3;
    u32 dummy10_5 : 16; // unused
    u32 redrawMessageBox : 1;
    u32 dummy10_22 : 10; // unused
    BgConfig *bgConfig;
    OptionsMenu options;
    Options *saveOptionsUnused; // unused copy of saveOptions
    MenuInputStateMgr *menuInputPtr;
    Options *saveOptions;
    MsgData *msgData;
    u8 filler2C[0x8];
    union {
        Window asArray[5];
        struct {
            Window title;
            Window entries;
            Window description;
            Window quitButton;
            Window confirmButton;
        };
    } windows;
    OptionsMenuEntry entries[MAX_ENTRIES];
    SpriteSystem *spriteRenderer;
    SpriteManager *spriteGfxHandler;
    Sprite *sprites[9];
    u8 filler2FC[36];
    u32 menuInputState;
    String *frameNumText;
    u8 textPrinter;
} OptionsMenuData; // size: 0x32c

static const s8 sEntryXOffsets[MAX_ENTRIES] = {
    0,
    0,
    0,
    0,
    0,
    -0x10,
    0,
};

static const u32 sOptionsAppBgLayers[5] = {
    GF_BG_LYR_MAIN_0,
    GF_BG_LYR_MAIN_1,
    GF_BG_LYR_MAIN_2,
    GF_BG_LYR_SUB_0,
    GF_BG_LYR_SUB_1,
};

static const int sNumChoicesPerEntry[MAX_ENTRIES] = {
    3, 2, 2, 2, 2, 20, 2
};

static const int sEntryBorderYCoords[MAX_ENTRIES] = {
    -8, -32, -56, -80, -104, -128, -156
};

static const u16 sChoiceXCoords[MAX_ENTRIES][3] = {
    { 124, 172, 220 },
    { 124, 172, 0   },
    { 132, 212, 0   },
    { 132, 212, 0   },
    { 132, 212, 0   },
    { 172, 0,   0   },
    { 0,   0,   0   },
};

static const int sActiveButtonXCoords[5][3] = {
    { 112, 160, 208 },
    { 112, 160, 208 },
    { 112, 192, 0   },
    { 112, 192, 0   },
    { 112, 192, 0   },
};

static const TouchscreenHitbox sOptionsAppTouchscreenHitboxes[16] = {
    { .rect = { .top = 26, .bottom = 46, .left = 112, .right = 151 } },
    { .rect = { .top = 26, .bottom = 46, .left = 160, .right = 200 } },
    { .rect = { .top = 26, .bottom = 46, .left = 208, .right = 248 } },
    { .rect = { .top = 50, .bottom = 70, .left = 112, .right = 151 } },
    { .rect = { .top = 50, .bottom = 70, .left = 160, .right = 200 } },
    { .rect = { .top = 74, .bottom = 93, .left = 112, .right = 167 } },
    { .rect = { .top = 74, .bottom = 93, .left = 192, .right = 247 } },
    { .rect = { .top = 98, .bottom = 117, .left = 112, .right = 167 } },
    { .rect = { .top = 98, .bottom = 117, .left = 192, .right = 247 } },
    { .rect = { .top = 122, .bottom = 142, .left = 112, .right = 167 } },
    { .rect = { .top = 122, .bottom = 142, .left = 192, .right = 247 } },
    { .rect = { .top = 146, .bottom = 166, .left = 110, .right = 150 } },
    { .rect = { .top = 146, .bottom = 166, .left = 208, .right = 253 } },
    { .rect = { .top = 172, .bottom = 191, .left = 128, .right = 180 } },
    { .rect = { .top = 172, .bottom = 191, .left = 183, .right = 255 } },
    { TOUCHSCREEN_RECTLIST_END },
};

static const u32 sTouchHitboxActions[15][2] = {
    { ENTRY_TEXT_SPEED,       0 },
    { ENTRY_TEXT_SPEED,       1 },
    { ENTRY_TEXT_SPEED,       2 },
    { ENTRY_BATTLE_SCENE,     0 },
    { ENTRY_BATTLE_SCENE,     1 },
    { ENTRY_BATTLE_STYLE,     0 },
    { ENTRY_BATTLE_STYLE,     1 },
    { ENTRY_SOUND_MODE,       0 },
    { ENTRY_SOUND_MODE,       1 },
    { ENTRY_BUTTON_MODE,      0 },
    { ENTRY_BUTTON_MODE,      1 },
    { ENTRY_MESSAGE_BOX_FRAME, 3 },
    { ENTRY_MESSAGE_BOX_FRAME, 4 },
    { ENTRY_CLOSE,            5 },
    { ENTRY_CLOSE,            6 },
};

static const UnmanagedSpriteTemplate ov54_021E6EAC[9] = {
    {
     .resourceSet = 0,
     .x = 112,
     .y = 24,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 0,
     .x = 112,
     .y = 48,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 0,
     .x = 112,
     .y = 72,
     .z = 0,
     .animation = 1,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 0,
     .x = 112,
     .y = 96,
     .z = 0,
     .animation = 1,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 0,
     .x = 112,
     .y = 120,
     .z = 0,
     .animation = 1,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 115,
     .y = 144,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 2,
     .x = 213,
     .y = 144,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 3,
     .x = 188,
     .y = 170,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 1,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 3,
     .x = 116,
     .y = 170,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 1,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
};

static void SetVRAMBanks(void);
static void OptionsMenuVBlank(OptionsMenuData *menuData);
static BOOL SetupMenuVisuals(OptionsMenuData *menuData);
static BOOL TeardownMenuData(OptionsMenuData *menuData);
static void SetupBgs(OptionsMenuData *menuData);
static void TeardownBgs(OptionsMenuData *menuData);
static void LoadBgTiles(OptionsMenuData *menuData);
static void TeardownTilemaps(OptionsMenuData *menuData);
static void SetupWindows(OptionsMenuData *menuData);
static void TeardownWindows(OptionsMenuData *menuData);
static void PrintTextFrameString(OptionsMenuData *menuData, String *frameNumText, BOOL instantTextSpeed);
static void PrintTitleAndEntries(OptionsMenuData *menuData);
static void LoadAllEntryChoices(OptionsMenuData *menuData);
static void PrintEntryChoices(OptionsMenuData *menuData, u16 entry);
static void OptionsApp_UpdateMenuEntryCarousel(OptionsMenuData *menuData, u32 entry, OptionsMenuEntry *menuEntry, s32 offset);
static void OptionsApp_HandleKeyInput(OptionsMenuData *menuData, OptionsMenuEntry *menuEntry);
static void OptionsApp_HandleInput(OptionsMenuData *menuData);
static void ov54_021E69D4(OptionsMenuData *menuData, u32 entry);
static void ov54_021E6A64(OptionsMenuData *menuData);
static void OptionsApp_SetupSpriteRenderer(OptionsMenuData *menuData);
static void OptionsApp_FreeSpriteRenderer(OptionsMenuData *menuData);
static void OptionsApp_SetupSprites(OptionsMenuData *menuData);
static void OptionsApp_SetActiveButtonsXPosition(OptionsMenuData *menuData);
static BOOL OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(OptionsMenuData *menuData);

BOOL OptionsMenu_Init(OverlayManager *manager, int *state) {
    OptionsMenuArgs *args = OverlayManager_GetArgs(manager);
    Heap_Create(HEAP_ID_3, HEAP_ID_OPTIONS_APP, 0x30000);

    OptionsMenuData *menuData = OverlayManager_CreateAndGetData(manager, sizeof(OptionsMenuData), HEAP_ID_OPTIONS_APP);
    memset(menuData, 0, sizeof(OptionsMenuData));

    menuData->options.textSpeed = Options_GetTextSpeed(args->options);
    menuData->options.battleScene = Options_GetBattleScene(args->options);
    menuData->options.battleStyle = Options_GetBattleStyle(args->options);
    menuData->options.soundMode = Options_GetSoundMethod(args->options);
    menuData->options.buttonMode = Options_GetButtonMode(args->options);
    menuData->options.messageBoxStyle = Options_GetFrame(args->options);

    menuData->menuInputPtr = args->menuInputStatePtr;
    menuData->saveOptionsUnused = args->options;
    menuData->heapID = HEAP_ID_OPTIONS_APP;
    menuData->saveOptions = args->options;
    menuData->menuInputState = MenuInputStateMgr_GetState(menuData->menuInputPtr);
    menuData->frameNumText = String_New(40, menuData->heapID);

    TextFlags_SetCanABSpeedUpPrint(FALSE);
    TextFlags_SetCanTouchSpeedUpPrint(FALSE);

    sub_0200FBF4(PM_LCD_TOP, RGB_BLACK);
    sub_0200FBF4(PM_LCD_BOTTOM, RGB_BLACK);

    return TRUE;
}

BOOL OptionsMenu_Exit(OverlayManager *manager, int *state) {
    OptionsMenuData *menuData = OverlayManager_GetData(manager);

    if (menuData->saveSelections == 1) {
        menuData->options.textSpeed = menuData->entries[ENTRY_TEXT_SPEED].selected;
        menuData->options.battleScene = menuData->entries[ENTRY_BATTLE_SCENE].selected;
        menuData->options.battleStyle = menuData->entries[ENTRY_BATTLE_STYLE].selected;
        menuData->options.soundMode = menuData->entries[ENTRY_SOUND_MODE].selected;
        menuData->options.buttonMode = menuData->entries[ENTRY_BUTTON_MODE].selected;
        menuData->options.messageBoxStyle = menuData->entries[ENTRY_MESSAGE_BOX_FRAME].selected;

        Options_SetTextSpeed(menuData->saveOptions, menuData->options.textSpeed);
        Options_SetBattleScene(menuData->saveOptions, menuData->options.battleScene);
        Options_SetBattleStyle(menuData->saveOptions, menuData->options.battleStyle);
        Options_SetSoundMethod(menuData->saveOptions, menuData->options.soundMode);
        Options_SetButtonMode(menuData->saveOptions, menuData->options.buttonMode);
        Options_SetFrame(menuData->saveOptions, menuData->options.messageBoxStyle);
    } else if (menuData->saveSelections == 2) {
        GF_SndSetMonoFlag(menuData->options.soundMode);
        Options_SetButtonModeOnMain(NULL, menuData->options.buttonMode);
        Options_SetTextSpeed(menuData->saveOptions, menuData->options.textSpeed);
    }

    String_Delete(menuData->frameNumText);

    TextFlags_SetCanABSpeedUpPrint(TRUE);
    TextFlags_SetCanTouchSpeedUpPrint(TRUE);

    OverlayManager_FreeData(manager);
    Heap_Destroy(menuData->heapID);

    return TRUE;
}

BOOL OptionsMenu_Main(OverlayManager *manager, int *state) {
    OptionsMenuData *menuData = OverlayManager_GetData(manager);
    switch (menuData->state) {
    case 0:
        if (!SetupMenuVisuals(menuData)) {
            return FALSE;
        }

        menuData->dummy0C = 0;
        BeginNormalPaletteFade(0, 1, 1, RGB_BLACK, 6, 1, menuData->heapID);
        OptionsApp_SetActiveButtonsXPosition(menuData);
        SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
        break;
    case 1:
        SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
        if (!IsPaletteFadeFinished()) {
            return FALSE;
        }
        break;
    case 2:
        if (menuData->saveSelections != 0) {
            SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
            break;
        }
        OptionsApp_HandleInput(menuData);
        SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
        return FALSE;
    case 3:
        SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
        if (!OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(menuData)) {
            menuData->dummy0C = 0;
            BeginNormalPaletteFade(0, 0, 0, RGB_BLACK, 6, 1, menuData->heapID);
            break;
        }
        return FALSE;
    case 4:
        if (TextPrinterCheckActive(menuData->textPrinter)) {
            RemoveTextPrinter(menuData->textPrinter);
        }
        SpriteSystem_DrawSprites(menuData->spriteGfxHandler);
        if (!IsPaletteFadeFinished()) {
            return FALSE;
        }
        break;
    case 5:
        if (TeardownMenuData(menuData)) {
            return TRUE;
        }
        return FALSE;
    }

    menuData->state++;
    return FALSE;
}

static void SetVRAMBanks(void) {
    GraphicsBanks banks = {
        .bg = GX_VRAM_BG_128_A,
        .subbg = GX_VRAM_SUB_BG_128_C,
        .obj = GX_VRAM_OBJ_16_G,
        .subobj = GX_VRAM_SUB_OBJ_16_I,
    };
    GfGfx_SetBanks(&banks);
}

static void OptionsMenuVBlank(OptionsMenuData *menuData) {
    if (menuData->redrawMessageBox) {
        LoadUserFrameGfx2(menuData->bgConfig, GF_BG_LYR_SUB_1, 0x6D, 15, menuData->entries[ENTRY_MESSAGE_BOX_FRAME].selected, menuData->heapID);
        menuData->redrawMessageBox = FALSE;
    }

    SpriteSystem_TransferOam();
    NNS_GfdDoVramTransfer();
    DoScheduledBgGpuUpdates(menuData->bgConfig);
    OS_SetIrqCheckFlag(OS_IE_VBLANK);
}

static BOOL SetupMenuVisuals(OptionsMenuData *menuData) {
    switch (menuData->subState) {
    case 0:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();

        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        GX_SetVisiblePlane(GX_PLANEMASK_NONE);
        GXS_SetVisiblePlane(GX_PLANEMASK_NONE);

        SetVRAMBanks();

        GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN);

        sub_0200FBDC(0);
        sub_0200FBDC(1);

        SetupBgs(menuData);
        OptionsApp_SetupSpriteRenderer(menuData);
        break;

    case 1:
        LoadBgTiles(menuData);
        menuData->msgData = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, NARC_msg_msg_0045_bin, menuData->heapID);
        LoadAllEntryChoices(menuData);
        break;

    case 2:
        SetupWindows(menuData);
        PrintTitleAndEntries(menuData);
        GF_CreateVramTransferManager(32, menuData->heapID);
        GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
        sub_0203A964();
        OptionsApp_SetupSprites(menuData);

        Main_SetVBlankIntrCB((GFIntrCB)OptionsMenuVBlank, menuData);
        menuData->subState = 0;
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_ON);
        return TRUE;
    }

    menuData->subState++;
    return FALSE;
}

static BOOL TeardownMenuData(OptionsMenuData *menuData) {
    switch (menuData->subState) {
    case 0:
        GF_DestroyVramTransferManager();
        TeardownWindows(menuData);

        for (int i = 0; i < MAX_ENTRIES - 1; i++) {
            for (int j = 0; j < menuData->entries[i].numChoices; j++) {
                String_Delete(menuData->entries[i].choices[j]);
            }
        }

        DestroyMsgData(menuData->msgData);
        TeardownTilemaps(menuData);
        TeardownBgs(menuData);
        OptionsApp_FreeSpriteRenderer(menuData);
        break;
    case 1:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();
        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        GX_SetVisiblePlane(GX_PLANEMASK_NONE);
        GXS_SetVisiblePlane(GX_PLANEMASK_NONE);
        menuData->subState = 0;
        return TRUE;
    }

    menuData->subState++;
    return FALSE;
}

static void SetupBgs(OptionsMenuData *menuData) {
    menuData->bgConfig = BgConfig_Alloc(menuData->heapID);
    GraphicsModes modes = {
        .dispMode = GX_DISPMODE_GRAPHICS,
        .bgMode = GX_BGMODE_0,
        .subMode = GX_BGMODE_0,
        ._2d3dMode = GX_BG0_AS_2D,
    };
    SetBothScreensModesAndDisable(&modes);

    BgTemplate templates[5] = {
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 0,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf000,
         .charBase = GX_BG_CHARBASE_0x04000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 1,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xe800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 2,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 1,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf000,
         .charBase = GX_BG_CHARBASE_0x08000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 0,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
    };

    for (int i = 0; i < 5; i++) {
        InitBgFromTemplate(menuData->bgConfig, sOptionsAppBgLayers[i], &templates[i], GF_BG_TYPE_TEXT);
        BgClearTilemapBufferAndCommit(menuData->bgConfig, sOptionsAppBgLayers[i]);
    }

    BG_ClearCharDataRange(GF_BG_LYR_MAIN_0, 32, 0, menuData->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_1, 32, 0, menuData->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 32, 0, menuData->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 32, 0, menuData->heapID);
}

static void TeardownBgs(OptionsMenuData *menuData) {
    FreeBgTilemapBuffer(menuData->bgConfig, GF_BG_LYR_SUB_1);
    FreeBgTilemapBuffer(menuData->bgConfig, GF_BG_LYR_SUB_0);
    FreeBgTilemapBuffer(menuData->bgConfig, GF_BG_LYR_MAIN_2);
    FreeBgTilemapBuffer(menuData->bgConfig, GF_BG_LYR_MAIN_1);
    FreeBgTilemapBuffer(menuData->bgConfig, GF_BG_LYR_MAIN_0);
    Heap_Free(menuData->bgConfig);
}

static void LoadBgTiles(OptionsMenuData *menuData) {
    GfGfxLoader_GXLoadPal(NARC_a_0_7_2, 3, GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_0_OFFSET, 0x40, menuData->heapID);
    GfGfxLoader_LoadCharData(NARC_a_0_7_2, 8, menuData->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, menuData->heapID);
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 19, menuData->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, menuData->heapID);
    GfGfxLoader_GXLoadPal(NARC_a_0_7_2, 2, GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_0_OFFSET, 0x40, menuData->heapID);
    GfGfxLoader_LoadCharData(NARC_a_0_7_2, 7, menuData->bgConfig, GF_BG_LYR_MAIN_0, 0, 0, FALSE, menuData->heapID);
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 17, menuData->bgConfig, GF_BG_LYR_MAIN_2, 0, 0, FALSE, menuData->heapID);
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 18, menuData->bgConfig, GF_BG_LYR_MAIN_0, 0, 0, FALSE, menuData->heapID);

    BgSetPosTextAndCommit(menuData->bgConfig, GF_BG_LYR_MAIN_0, BG_POS_OP_SET_Y, sEntryBorderYCoords[menuData->cursor]);
}

static void TeardownTilemaps(OptionsMenuData *menuData) {
    // empty, maybe would've been used to free graphics data?
}

static void SetupWindows(OptionsMenuData *menuData) {
    AddWindowParameterized(menuData->bgConfig, &menuData->windows.title, GF_BG_LYR_MAIN_1, 1, 0, 12, 3, 13, 0xA);
    AddWindowParameterized(menuData->bgConfig, &menuData->windows.entries, GF_BG_LYR_MAIN_1, 1, 3, 30, 18, 13, 0x2E);
    AddWindowParameterized(menuData->bgConfig, &menuData->windows.quitButton, GF_BG_LYR_MAIN_1, 24, 21, 7, 3, 13, 0x24A);
    AddWindowParameterized(menuData->bgConfig, &menuData->windows.description, GF_BG_LYR_SUB_1, 2, 19, 27, 4, 12, 0x1);
    AddWindowParameterized(menuData->bgConfig, &menuData->windows.confirmButton, GF_BG_LYR_MAIN_1, 15, 21, 7, 3, 13, 0x25F);

    LoadUserFrameGfx2(menuData->bgConfig, GF_BG_LYR_SUB_1, 0x6D, 15, menuData->options.messageBoxStyle, menuData->heapID);
    LoadFontPal0(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_13_OFFSET, menuData->heapID);
    LoadFontPal0(GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_13_OFFSET, menuData->heapID);
    LoadFontPal1(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_12_OFFSET, menuData->heapID);
    LoadFontPal1(GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_12_OFFSET, menuData->heapID);

    FillWindowPixelBuffer(&menuData->windows.title, 0x00);
    FillWindowPixelBuffer(&menuData->windows.entries, 0x00);
    FillWindowPixelBuffer(&menuData->windows.quitButton, 0x00);
    FillWindowPixelBuffer(&menuData->windows.confirmButton, 0x00);
    FillWindowPixelBuffer(&menuData->windows.description, 0xFF);

    ClearWindowTilemap(&menuData->windows.description);
    ClearWindowTilemap(&menuData->windows.entries);
    ClearWindowTilemap(&menuData->windows.title);

    DrawFrameAndWindow2(&menuData->windows.description, TRUE, 0x6D, 15);
}

static void TeardownWindows(OptionsMenuData *menuData) {
    sub_0200E5D4(&menuData->windows.entries, FALSE);
    ClearFrameAndWindow2(&menuData->windows.description, FALSE);

    for (u16 i = 0; i < NELEMS(menuData->windows.asArray); i++) {
        ClearWindowTilemapAndCopyToVram(&menuData->windows.asArray[i]);
        FillWindowPixelBuffer(&menuData->windows.asArray[i], 0x00);
        ClearWindowTilemap(&menuData->windows.asArray[i]);
        RemoveWindow(&menuData->windows.asArray[i]);
    }
}

static void PrintTextFrameString(OptionsMenuData *menuData, String *frameNumText, BOOL instantTextSpeed) {
    u32 textFrameDelay = Options_GetTextFrameDelay(menuData->saveOptions);

    if (TextPrinterCheckActive(menuData->textPrinter)) {
        RemoveTextPrinter(menuData->textPrinter);
    }

    ReadMsgDataIntoString(menuData->msgData, msg_0045_00040 + menuData->entries[ENTRY_MESSAGE_BOX_FRAME].selected, frameNumText); // WINDOW TYPE XX

    FillWindowPixelBuffer(&menuData->windows.description, 0xFF);

    if (instantTextSpeed) {
        AddTextPrinterParameterizedWithColor(&menuData->windows.description, 1, frameNumText, 4, 0, TEXT_SPEED_INSTANT, MAKE_TEXT_COLOR(1, 2, 15), NULL);
    } else {
        menuData->textPrinter = AddTextPrinterParameterizedWithColor(&menuData->windows.description, 1, frameNumText, 4, 0, textFrameDelay, MAKE_TEXT_COLOR(1, 2, 15), NULL);
    }
}

static void PrintTitleAndEntries(OptionsMenuData *menuData) {
    u16 i;
    String *tmpString = String_New(40, menuData->heapID);

    ReadMsgDataIntoString(menuData->msgData, msg_0045_00000, tmpString); // OPTIONS
    AddTextPrinterParameterizedWithColor(&menuData->windows.title, 0, tmpString, 2, 5, TEXT_SPEED_INSTANT, MAKE_TEXT_COLOR(15, 2, 0), NULL);

    String_SetEmpty(tmpString);
    PrintTextFrameString(menuData, tmpString, TRUE);

    for (i = 0; i < MAX_ENTRIES - 1; i++) {
        String_SetEmpty(tmpString);
        ReadMsgDataIntoString(menuData->msgData, msg_0045_00001 + i, tmpString); // Option names
        AddTextPrinterParameterizedWithColor(&menuData->windows.entries, 0, tmpString, 4, i * 24 + 5, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    }

    String_SetEmpty(tmpString);
    ReadMsgDataIntoString(menuData->msgData, msg_0045_00008, tmpString); // QUIT
    AddTextPrinterParameterizedWithColor(&menuData->windows.quitButton, 0, tmpString, 0, 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    String_SetEmpty(tmpString);
    ReadMsgDataIntoString(menuData->msgData, msg_0045_00007, tmpString); // CONFIRM
    AddTextPrinterParameterizedWithColor(&menuData->windows.confirmButton, 0, tmpString, 0, 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);

    for (i = 0; i < MAX_ENTRIES; i++) {
        PrintEntryChoices(menuData, i);
    }

    CopyWindowToVram(&menuData->windows.title);
    CopyWindowToVram(&menuData->windows.entries);
    CopyWindowToVram(&menuData->windows.quitButton);
    CopyWindowToVram(&menuData->windows.confirmButton);
    CopyWindowToVram(&menuData->windows.description);

    String_Delete(tmpString);
}

static void LoadAllEntryChoices(OptionsMenuData *menuData) {
    u16 i, j;
    u16 msgNum = 0;
    for (i = 0; i < MAX_ENTRIES - 1; i++) {
        menuData->entries[i].numChoices = sNumChoicesPerEntry[i];
        for (j = 0; j < sNumChoicesPerEntry[i]; j++) {
            menuData->entries[i].choices[j] = NewString_ReadMsgData(menuData->msgData, msg_0045_00009 + msgNum++); // Option values
        }
    }

    menuData->entries[ENTRY_TEXT_SPEED].selected = menuData->options.textSpeed;
    menuData->entries[ENTRY_BATTLE_SCENE].selected = menuData->options.battleScene;
    menuData->entries[ENTRY_BATTLE_STYLE].selected = menuData->options.battleStyle;
    menuData->entries[ENTRY_SOUND_MODE].selected = menuData->options.soundMode;
    menuData->entries[ENTRY_BUTTON_MODE].selected = menuData->options.buttonMode;
    menuData->entries[ENTRY_MESSAGE_BOX_FRAME].selected = menuData->options.messageBoxStyle;
    menuData->entries[ENTRY_CLOSE].selected = 0;
}

static void PrintEntryChoices(OptionsMenuData *menuData, u16 entry) {
    u32 selectedColor = MAKE_TEXT_COLOR(1, 2, 0);
    u32 notSelectedColor = MAKE_TEXT_COLOR(15, 2, 0);
    u32 color;
    u16 i;
    u8 frameDelay;
    u16 x = 0;
    FillWindowPixelRect(&menuData->windows.entries, 0, 108 + sEntryXOffsets[entry], entry * 24 + 5, 384, 24);

    switch (entry) {
    case ENTRY_MESSAGE_BOX_FRAME:
        x = sChoiceXCoords[entry][0] - FontID_String_GetWidth(0, menuData->entries[entry].choices[menuData->entries[entry].selected], 0) / 2;
        AddTextPrinterParameterizedWithColor(&menuData->windows.entries, 0, menuData->entries[entry].choices[menuData->entries[entry].selected], x, entry * 24 + 5, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        CopyWindowToVram(&menuData->windows.entries);
        PrintTextFrameString(menuData, menuData->frameNumText, TRUE);
        menuData->redrawMessageBox = TRUE;
        return;
    case ENTRY_SOUND_MODE:
        GF_SndSetMonoFlag(menuData->entries[entry].selected);
        break;
    case ENTRY_BUTTON_MODE:
        Options_SetButtonModeOnMain(NULL, menuData->entries[entry].selected);
        break;
    case ENTRY_TEXT_SPEED:
        Options_SetTextSpeed(menuData->saveOptions, menuData->entries[entry].selected);
        PrintTextFrameString(menuData, menuData->frameNumText, FALSE);
        break;
    }

    x = 0;
    for (i = 0; i < menuData->entries[entry].numChoices; i++) {
        if (i == menuData->entries[entry].selected) {
            color = selectedColor;
        } else {
            color = notSelectedColor;
        }
        // required to match a double `bls` above
        if (i == menuData->entries[entry].numChoices - 1) {
            frameDelay = TEXT_SPEED_NOTRANSFER;
        } else {
            frameDelay = TEXT_SPEED_NOTRANSFER;
        }
        x = sChoiceXCoords[entry][i] - (FontID_String_GetWidth(0, menuData->entries[entry].choices[i], 0) / 2);
        AddTextPrinterParameterizedWithColor(&menuData->windows.entries, 0, menuData->entries[entry].choices[i], x, entry * 24 + 5, frameDelay, color, NULL);
    }

    CopyWindowToVram(&menuData->windows.entries);
}

static void OptionsApp_UpdateMenuEntryCarousel(OptionsMenuData *menuData, u32 entry, OptionsMenuEntry *menuEntry, s32 offset) {
    if (entry == ENTRY_MESSAGE_BOX_FRAME) {
        if (offset == -1) {
            Sprite_SetAnimCtrlSeq(menuData->sprites[5], 1);
        } else if (offset == 1) {
            Sprite_SetAnimCtrlSeq(menuData->sprites[6], 1);
        }
    }

    if (offset > 0) {
        menuEntry->selected = (menuEntry->selected + offset) % menuEntry->numChoices;
    } else if (offset < 0) {
        menuEntry->selected = (menuEntry->selected + menuEntry->numChoices - 1) % menuEntry->numChoices;
    }
}

static void OptionsApp_HandleKeyInput(OptionsMenuData *menuData, OptionsMenuEntry *menuEntry) {
    if (menuData->cursor != ENTRY_CLOSE) {
        if (gSystem.newKeys & PAD_KEY_RIGHT) {
            OptionsApp_UpdateMenuEntryCarousel(menuData, menuData->cursor, menuEntry, 1);
            PrintEntryChoices(menuData, menuData->cursor);
            PlaySE(SEQ_SE_DP_SELECT);
        } else if (gSystem.newKeys & PAD_KEY_LEFT) {
            OptionsApp_UpdateMenuEntryCarousel(menuData, menuData->cursor, menuEntry, -1);
            PrintEntryChoices(menuData, menuData->cursor);
            PlaySE(SEQ_SE_DP_SELECT);
        }
        OptionsApp_SetActiveButtonsXPosition(menuData);
    } else {
        if (gSystem.newKeys & PAD_KEY_LEFT) {
            if (menuData->entries[menuData->cursor].selected == 0) {
                menuData->entries[menuData->cursor].selected = 1;
                ov54_021E69D4(menuData, menuData->cursor);
                PlaySE(SEQ_SE_DP_SELECT);
            }
        } else if (gSystem.newKeys & PAD_KEY_RIGHT) {
            if (menuData->entries[menuData->cursor].selected == 1) {
                menuData->entries[menuData->cursor].selected = 0;
                ov54_021E69D4(menuData, menuData->cursor);
                PlaySE(SEQ_SE_DP_SELECT);
            }
        }
    }

    if (gSystem.newKeys & PAD_KEY_UP) {
        menuData->cursor = (menuData->cursor + (MAX_ENTRIES - 1)) % MAX_ENTRIES;
        ov54_021E69D4(menuData, menuData->cursor);
        PlaySE(SEQ_SE_DP_SELECT);
    } else if (gSystem.newKeys & PAD_KEY_DOWN) {
        menuData->cursor = (menuData->cursor + 1) % MAX_ENTRIES;
        ov54_021E69D4(menuData, menuData->cursor);
        PlaySE(SEQ_SE_DP_SELECT);
    } else if ((gSystem.newKeys & PAD_BUTTON_A) && menuData->cursor == ENTRY_CLOSE) {
        if (menuData->entries[menuData->cursor].selected == 1) {
            MenuInputStateMgr_SetState(menuData->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
            PlaySE(SEQ_SE_DP_SAVE);
            Sprite_SetAnimCtrlSeq(menuData->sprites[8], 3);
            menuData->saveSelections = 1;
        } else {
            MenuInputStateMgr_SetState(menuData->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
            PlaySE(SEQ_SE_GS_GEARCANCEL);
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 3);
            menuData->saveSelections = 2;
        }
    } else if (gSystem.newKeys & PAD_BUTTON_B) {
        MenuInputStateMgr_SetState(menuData->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
        PlaySE(SEQ_SE_GS_GEARCANCEL);
        if (menuData->cursor == ENTRY_CLOSE && menuData->entries[menuData->cursor].selected == 0) {
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 3);
        } else {
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 2);
        }
        menuData->saveSelections = 2;
    }
}

static void OptionsApp_HandleInput(OptionsMenuData *menuData) {
    if (gSystem.touchNew != 0) {
        const int hitboxIndex = TouchscreenHitbox_FindRectAtTouchNew(sOptionsAppTouchscreenHitboxes);
        switch (hitboxIndex) {
        case -1:
            break;

        case 13: // Confirm button
            menuData->cursor = sTouchHitboxActions[hitboxIndex][0];
            OptionsApp_SetActiveButtonsXPosition(menuData);
            ov54_021E6A64(menuData);
            menuData->saveSelections = 1;
            PlaySE(SEQ_SE_DP_SAVE);
            menuData->menuInputState = 1;
            MenuInputStateMgr_SetState(menuData->menuInputPtr, MENU_INPUT_STATE_TOUCH);
            menuData->entries[menuData->cursor].selected = 1;
            ov54_021E69D4(menuData, menuData->cursor);
            Sprite_SetAnimCtrlSeq(menuData->sprites[8], 3);
            break;

        case 14: // Quit button
            menuData->cursor = sTouchHitboxActions[hitboxIndex][0];
            OptionsApp_SetActiveButtonsXPosition(menuData);
            ov54_021E6A64(menuData);
            menuData->saveSelections = 2;
            PlaySE(SEQ_SE_GS_GEARCANCEL);
            menuData->menuInputState = 1;
            MenuInputStateMgr_SetState(menuData->menuInputPtr, MENU_INPUT_STATE_TOUCH);
            menuData->entries[menuData->cursor].selected = 0;
            ov54_021E69D4(menuData, menuData->cursor);
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 3);
            break;

        default: {
            menuData->cursor = sTouchHitboxActions[hitboxIndex][0];
            OptionsMenuEntry *entry = &menuData->entries[menuData->cursor];

            u32 value = sTouchHitboxActions[hitboxIndex][1];
            if (value == 3) {
                OptionsApp_UpdateMenuEntryCarousel(menuData, menuData->cursor, entry, -1);
            } else if (value == 4) {
                OptionsApp_UpdateMenuEntryCarousel(menuData, menuData->cursor, entry, 1);
            } else {
                entry->selected = value;
            }
            PrintEntryChoices(menuData, menuData->cursor);
            ov54_021E69D4(menuData, menuData->cursor);
            OptionsApp_SetActiveButtonsXPosition(menuData);
            ov54_021E6A64(menuData);
            menuData->menuInputState = 1;
            PlaySE(SEQ_SE_DP_SELECT);
            break;
        }
        }
    } else if (gSystem.newKeys != 0) {
        OptionsApp_HandleKeyInput(menuData, &menuData->entries[menuData->cursor]);
    }
}

static void ov54_021E69D4(OptionsMenuData *menuData, u32 entry) {
    if (entry == ENTRY_CLOSE) {
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_OFF);
        if (menuData->entries[entry].selected == 0) {
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 1);
            Sprite_SetAnimCtrlSeq(menuData->sprites[8], 0);
        } else {
            Sprite_SetAnimCtrlSeq(menuData->sprites[7], 0);
            Sprite_SetAnimCtrlSeq(menuData->sprites[8], 1);
        }
    } else {
        BgSetPosTextAndCommit(menuData->bgConfig, GF_BG_LYR_MAIN_0, BG_POS_OP_SET_Y, sEntryBorderYCoords[menuData->cursor]);
        Sprite_SetAnimCtrlSeq(menuData->sprites[7], 0);
        Sprite_SetAnimCtrlSeq(menuData->sprites[8], 0);
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_ON);
    }
}

static void ov54_021E6A64(OptionsMenuData *menuData) {
    if (menuData->cursor == ENTRY_CLOSE) {
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_OFF);
    }
}

static void OptionsApp_SetupSpriteRenderer(OptionsMenuData *menuData) {
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);

    menuData->spriteRenderer = SpriteSystem_Alloc(menuData->heapID);
    menuData->spriteGfxHandler = SpriteManager_New(menuData->spriteRenderer);

    const OamManagerParam unk1 = {
        .fromOBJmain = 0,
        .numOBJmain = 128,
        .fromAffineMain = 0,
        .numAffineMain = 32,
        .fromOBJsub = 0,
        .numOBJsub = 128,
        .fromAffineSub = 0,
        .numAffineSub = 32,
    };
    const OamCharTransferParam unk2 = {
        .maxTasks = 9,
        .sizeMain = 0x400,
        .sizeSub = 0x400,
        .charModeMain = GX_OBJVRAMMODE_CHAR_1D_32K,
        .charModeSub = GX_OBJVRAMMODE_CHAR_1D_32K,
    };
    SpriteSystem_Init(menuData->spriteRenderer, &unk1, &unk2, 32);
    SpriteSystem_InitSprites(menuData->spriteRenderer, menuData->spriteGfxHandler, 9);

    u16 fileIdList[7] = {
        NARC_resdat_resdat_00000022_bin,
        NARC_resdat_resdat_00000023_bin,
        NARC_resdat_resdat_00000021_bin,
        NARC_resdat_resdat_00000020_bin,
        0xFFFF,
        0xFFFF,
        NARC_resdat_resdat_00000077_bin,
    };
    sub_0200D294(menuData->spriteRenderer, menuData->spriteGfxHandler, fileIdList);

    G2dRenderer_SetSubSurfaceCoords(SpriteSystem_GetRenderer(menuData->spriteRenderer), FX32_CONST(0), FX32_CONST(256));
}

static void OptionsApp_FreeSpriteRenderer(OptionsMenuData *menuData) {
    SpriteSystem_DestroySpriteManager(menuData->spriteRenderer, menuData->spriteGfxHandler);
    SpriteSystem_Free(menuData->spriteRenderer);
    menuData->spriteGfxHandler = NULL;
}

static void OptionsApp_SetupSprites(OptionsMenuData *menuData) {
    for (u16 i = 0; i < NELEMS(menuData->sprites); i++) {
        menuData->sprites[i] = SpriteSystem_CreateSpriteFromResourceHeader(menuData->spriteRenderer, menuData->spriteGfxHandler, &ov54_021E6EAC[i]);
        thunk_Sprite_SetPriority(menuData->sprites[i], 2);
        Sprite_SetAnimActiveFlag(menuData->sprites[i], TRUE);
    }

    Sprite_SetDrawFlag(menuData->sprites[7], TRUE);
}

static void OptionsApp_SetActiveButtonsXPosition(OptionsMenuData *menuData) {
    for (int i = 0; i < 5; i++) {
        s16 x, y;
        Sprite_GetPositionXY(menuData->sprites[i], &x, &y);
        x = sActiveButtonXCoords[i][menuData->entries[i].selected];
        Sprite_SetPositionXY(menuData->sprites[i], x, y);
    }
}

static BOOL OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(OptionsMenuData *menuData) {
    if (Sprite_IsAnimated(menuData->sprites[7]) == 0 && Sprite_IsAnimated(menuData->sprites[8]) == 0) {
        return FALSE;
    }

    return TRUE;
}
