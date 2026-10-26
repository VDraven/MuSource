#include "ui/session/UiSessionLogic.h"
#include "I18N/All.h"
#include "app/AppWindow.h"
#include "app/ApplicationAudio.h"
#include "app/ApplicationKeeper.h"
#include "app/ApplicationLoopFrame.h"
#include "app/ApplicationNetwork.h"
#include "data/ItemData.h"
#include "data/Localization.h"
#include "domain/CharacterPresentation.h"
#include "domain/CharacterSystem.h"
#include "domain/Events.h"
#include "domain/ItemsSkills.h"
#include "domain/MapSimulation.h"
#include "domain/MovementAI.h"
#include "domain/Shop.h"
#include "domain/WorldSimulation.h"
#include "network/generated/PacketFunctions_ClientToServer.h"
#include "render/ModelResources.h"
#include "render/Sprites.h"
#include "render/Textures.h"
#include "render/UiAdapter.h"
#include "render/World.h"
#include "session/SessionGameplay.h"
#include "session/SessionKeeper.h"
#include "session/SessionNetwork.h"
#include "session/SessionPresentation.h"
#include "session/SessionRender.h"
#include "session/SessionUi.h"
#include "session/SessionWorkspace.h"
#include "support/Camera.h"
#include "support/CoreMath.h"
#include "ui/features/Activities/ActivitiesLogic.h"
#include "ui/features/Dialogs/DialogsLogic.h"
#include "ui/features/Hud/HudLogic.h"
#include "ui/features/Items/ItemsLogic.h"
#include "ui/features/Items/ItemsRender.h"
#include "ui/features/QuestNpc/QuestNpcLogic.h"
#include "ui/features/Shell/ShellLogic.h"
#include "ui/features/Social/SocialLogic.h"
#include "ui/features/World/WorldLogic.h"
#include "ui/runtime/UiControls.h"
#include "ui/runtime/UiRuntime.h"
#include "ui/session/UiSessionRender.h"

//  UIManager.cpp
CUIManager::CUIManager(SessionKeeper &keeper)
    : SessionLegacyCalls(keeper), g_hWnd(keeper.PlatformWindowHandle()),
      g_ErrorReport(keeper.ErrorReport()), focusedTextInputBox_(keeper.FocusedTextInputBox())
{
    g_pUIGateKeeper = new CUIGateKeeper;
    g_pUIPopup = new CUIPopup(keeper);
    g_pUIJewelHarmonyinfo = JewelHarmonyInfo::MakeInfo(keeper);
    g_pItemAddOptioninfo = ItemAddOptioninfo::MakeInfo(keeper);

    Init();
}

CUIManager::~CUIManager()
{
    SAFE_DELETE(g_pItemAddOptioninfo);
    SAFE_DELETE(g_pUIJewelHarmonyinfo);
    SAFE_DELETE(g_pUIGateKeeper);
    SAFE_DELETE(g_pUIPopup);
}

void SessionUiUnit::InitializeLegacyManager()
{
    LegacyUiManager().InitializeSession();
    g_pUIManager = new CUIManager(sessionKeeper_);
}

void SessionUiUnit::ShutdownLegacyManager() noexcept
{
    SAFE_DELETE(g_pUIManager);
    LegacyUiManager().ShutdownSession();
}

void CUIManager::Init()
{
    g_pUIPopup->Init();
    InitPetManager();
    ClearPersonalShop();
}

bool CUIManager::PressKey(int nKey)
{
    return false;
}

bool CUIManager::IsInputEnable()
{
    if (InputEnable || GuildInputEnable ||
        (g_pUIPopup->GetPopupID() != 0 && g_pUIPopup->IsInputEnable()))
        return true;
    // A focused portable text field captures the keyboard (issue #447). It no
    // longer takes Win32 focus, so GetFocus() stays on the main window; report
    // "input active" explicitly so callers suppress world/camera keys while typing.
    if (focusedTextInputBox_ != nullptr)
        return true;
    if (GetFocus() == g_hWnd)
        return false;
    return true;
}

void CUIManager::UpdateInput()
{
}

void CUIManager::CloseAll()
{
    for (DWORD dwInterface = ::INTERFACE_FRIEND; dwInterface < INTERFACE_MAX_COUNT; ++dwInterface)
    {
        if (g_pUIManager->IsOpen(dwInterface))
        {
            Close(dwInterface);
        }
    }

    g_pUIPopup->CancelPopup();
}

bool CUIManager::CloseInterface(std::list<DWORD> &dwInterfaceFlag, DWORD dwExtraData)
{
    return true;
}

bool CUIManager::IsOpen(DWORD dwInterface)
{
    if (dwInterface == 0)
    {
        for (DWORD dwInterface = ::INTERFACE_FRIEND; dwInterface < INTERFACE_MAX_COUNT;
             ++dwInterface)
        {
            if (IsOpen(dwInterface))
                return true;
        }
    }

    switch (dwInterface)
    {
    case ::INTERFACE_INVENTORY:
        return HeroInventoryEnable;
    case ::INTERFACE_STORAGE:
        return StorageInventoryEnable;
    case INTERFACE_PERSONALSHOPSALE:
    case INTERFACE_PERSONALSHOPPURCHASE:
        return g_bPersonalShopWnd;
    case ::INTERFACE_SERVERDIVISION:
        return g_bServerDivisionEnable;
    default:
        return false;
    }
    return false;
}

bool CUIManager::IsCanOpen(DWORD dwInterfaceFlag)
{
    return true;
}

void CUIManager::GetInterfaceAll(std::list<DWORD> &outflag)
{
    for (DWORD flag = ::INTERFACE_FRIEND; flag < INTERFACE_MAX_COUNT; ++flag)
    {
        outflag.push_back(flag);
    }
}

void CUIManager::GetInsertInterface(std::list<DWORD> &outflag, DWORD insertflag)
{
    outflag.push_back(insertflag);
}

void CUIManager::GetDeleteInterface(std::list<DWORD> &outflag, DWORD deleteflag)
{
    for (auto iter = outflag.begin(); iter != outflag.end();)
    {
        auto Tempiter = iter;
        ++iter;
        DWORD Tempflag = *Tempiter;

        if (Tempflag == deleteflag)
        {
            outflag.erase(Tempiter);
        }
    }
}

bool CUIManager::Open(DWORD dwInterface, DWORD dwExtraData)
{
    if (IsOpen(::INTERFACE_REFINERYINFO))
        return false;
    if (IsOpen(dwInterface))
        return false;
    if (IsCanOpen(dwInterface) == false)
        return false;
    if (LogOut == true)
        return false;

    std::list<DWORD> closeinterfaceflag;
    GetInterfaceAll(closeinterfaceflag);
    GetDeleteInterface(closeinterfaceflag, dwInterface);
    GetDeleteInterface(closeinterfaceflag, ::INTERFACE_FRIEND);

    switch (dwInterface)
    {
    case ::INTERFACE_INVENTORY: {
        bool bResult = CloseInterface(closeinterfaceflag);
        if (bResult)
        {
            HeroInventoryEnable = true;
        }
    }
    break;
    case INTERFACE_PERSONALSHOPSALE: {
        GetDeleteInterface(closeinterfaceflag, ::INTERFACE_INVENTORY);

        bool bResult = CloseInterface(closeinterfaceflag);
        if (bResult)
        {
            Open(::INTERFACE_INVENTORY);

            if (g_iPShopWndType != PSHOPWNDTYPE_NONE)
            {
                g_ErrorReport.Write(L"@ OpenPersonalShop : SendRequestInventory\n");
                SocketClient->ToGameServer()->SendInventoryRequest();
            }

            CreatePersonalItemTable();

            g_bPersonalShopWnd = true;
            g_iPShopWndType = PSHOPWNDTYPE_SALE;
        }
    }
    break;
    case INTERFACE_PERSONALSHOPPURCHASE: {
        bool bResult = CloseInterface(closeinterfaceflag);
        if (bResult)
        {
            Open(::INTERFACE_INVENTORY);

            if (g_iPShopWndType != PSHOPWNDTYPE_NONE)
            {
                g_ErrorReport.Write(L"@ OpenPersonalShop : SendRequestInventory\n");
                SocketClient->ToGameServer()->SendInventoryRequest();
            }
            CreatePersonalItemTable();

            g_bPersonalShopWnd = true;
            g_iPShopWndType = PSHOPWNDTYPE_PURCHASE;
        }
    }
    break;
    case ::INTERFACE_SERVERDIVISION: {
        bool bResult = CloseInterface(closeinterfaceflag);
        if (bResult)
        {
            g_bServerDivisionEnable = true;
            g_bServerDivisionAccept = false;
        }
    }
    break;
    default:
        return false;
    }

    PlayBuffer(SOUND_CLICK01);
    PlayBuffer(SOUND_INTERFACE01);

    return true;
}

bool CUIManager::Close(DWORD dwInterface, DWORD dwExtraData)
{
    if (!IsOpen(dwInterface))
        return false;

    switch (dwInterface)
    {
    case ::INTERFACE_INVENTORY: {
        bool bResult = true;
        if (bResult)
        {
            std::list<DWORD> closeinterfaceflag;

            GetInsertInterface(closeinterfaceflag, ::INTERFACE_TRADE);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_STORAGE);
            GetInsertInterface(closeinterfaceflag, INTERFACE_GUILDSTORAGE);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_MIXINVENTORY);
            GetInsertInterface(closeinterfaceflag, INTERFACE_PERSONALSHOPSALE);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_NPCBREEDER);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_NPCSHOP);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_SENATUS);
            GetInsertInterface(closeinterfaceflag, ::INTERFACE_REFINERY);
            bResult = CloseInterface(closeinterfaceflag, dwExtraData);
            if (bResult)
            {
                std::list<DWORD> closeflag;
                GetInsertInterface(closeflag, dwInterface);
                CloseInterface(closeflag, dwExtraData);
            }
        }
    }
    break;
    case ::INTERFACE_STORAGE:
    case INTERFACE_GUILDSTORAGE:
    case ::INTERFACE_MIXINVENTORY:
    case ::INTERFACE_TRADE:
    case ::INTERFACE_NPCBREEDER:
    case ::INTERFACE_NPCSHOP:
    case ::INTERFACE_GUARDSMAN: {
        std::list<DWORD> closeinterfaceflag;
        GetInsertInterface(closeinterfaceflag, dwInterface);
        bool bResult = CloseInterface(closeinterfaceflag, dwExtraData);
        if (bResult)
        {
            std::list<DWORD> closeflag;
            GetInsertInterface(closeflag, ::INTERFACE_INVENTORY);
            CloseInterface(closeflag, dwExtraData);
        }
    }
    break;
    default: {
        std::list<DWORD> closeinterfaceflag;
        GetInsertInterface(closeinterfaceflag, dwInterface);
        CloseInterface(closeinterfaceflag, dwExtraData);
    }
    break;
    }

    PlayBuffer(SOUND_CLICK01);
    PlayBuffer(SOUND_INTERFACE01);

    return true;
}

#define DOCK_EXTENT 10

//#define	UIM_TS_BG_BLACK		0
#define UIM_TS_BACK0 0
#define UIM_TS_BACK1 1
#define UIM_TS_121518 3
#define UIM_TS_BACK2 5
#define UIM_TS_BACK3 6
#define UIM_TS_BACK4 7
#define UIM_TS_BACK5 8
#define UIM_TS_BACK6 9
#define UIM_TS_BACK7 10
#define UIM_TS_BACK8 11
#define UIM_TS_BACK9 12
#define UIM_TS_MAX 13

CUIMng::CUIMng(SessionKeeper &keeper)
    : SessionLegacyCalls(keeper), sessionKeeper_(keeper),
      renderer_(keeper.RendererForConstruction()), input_(keeper.ApplicationInputForConstruction()),
      WindowWidth(keeper.PlatformWindowWidth()), WindowHeight(keeper.PlatformWindowHeight()),
      SceneFlag(keeper.InterfaceStorage().SceneFlag)
{
    m_asprTitle = NULL;
    m_pgbLoding = NULL;
    m_pLoadingScene = NULL;
}

CUIMng::~CUIMng()
{
    ShutdownSession();
}

void CUIMng::InitializeSession()
{
    m_MsgWin = std::make_unique<CMsgWin>(sessionKeeper_);
    m_SysMenuWin = std::make_unique<CSysMenuWin>(sessionKeeper_);
    m_LoginMainWin = std::make_unique<CLoginMainWin>(sessionKeeper_);
    m_ServerSelWin = std::make_unique<CServerSelWin>(sessionKeeper_);
    m_LoginWin = std::make_unique<CLoginWin>(sessionKeeper_);
    m_CreditWin = std::make_unique<CCreditWin>(sessionKeeper_);
    m_CharSelMainWin = std::make_unique<CCharSelMainWin>(sessionKeeper_);
    m_CharMakeWin = std::make_unique<CCharMakeWin>(sessionKeeper_);
    m_CharInfoBalloonMng = std::make_unique<CCharInfoBalloonMng>(sessionKeeper_);
    m_ServerMsgWin = std::make_unique<CServerMsgWin>(sessionKeeper_);
}

void CUIMng::ShutdownSession() noexcept
{
    Release();
    m_ServerMsgWin.reset();
    m_CharInfoBalloonMng.reset();
    m_LoginWin.reset();
    m_ServerSelWin.reset();
    m_LoginMainWin.reset();
    m_CreditWin.reset();
    m_CharSelMainWin.reset();
    m_CharMakeWin.reset();
    m_SysMenuWin.reset();
    m_MsgWin.reset();
}

void CUIMng::CreateTitleSceneUI()
{
    ReleaseTitleSceneUI();

    float fScaleX = static_cast<float>(WindowWidth) / 800.0f;
    float fScaleY = static_cast<float>(WindowHeight) / 600.0f;

    titleSprites_ = std::make_unique<SessionBoundArray<CSprite, UIM_TS_MAX>>(sessionKeeper_);
    m_asprTitle = titleSprites_->data();

    float _fScaleXTemp = static_cast<float>(WindowWidth) / 1280.0f;
    float _fScaleYTemp = static_cast<float>(WindowHeight) / 1024.0f;

    m_asprTitle[UIM_TS_BACK0].Create(400, 69, BITMAP_TITLE, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, fScaleX, fScaleY);
    m_asprTitle[UIM_TS_BACK0].SetPosition(0, 0);

    m_asprTitle[UIM_TS_BACK1].Create(400, 69, BITMAP_TITLE + 1, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, fScaleX, fScaleY);
    m_asprTitle[UIM_TS_BACK1].SetPosition(400, 0);

    m_asprTitle[UIM_TS_BACK2].Create(400, 100, BITMAP_TITLE + 6, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, fScaleX, fScaleY);
    m_asprTitle[UIM_TS_BACK2].SetPosition(0, 500);

    m_asprTitle[UIM_TS_BACK3].Create(400, 100, BITMAP_TITLE + 7, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, fScaleX, fScaleY);
    m_asprTitle[UIM_TS_BACK3].SetPosition(400, 500);

    m_asprTitle[UIM_TS_BACK4].Create(512, 512, BITMAP_TITLE + 8, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK4].SetPosition(0, 119);

    m_asprTitle[UIM_TS_BACK5].Create(512, 512, BITMAP_TITLE + 9, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK5].SetPosition(512, 119);

    m_asprTitle[UIM_TS_BACK6].Create(256, 512, BITMAP_TITLE + 10, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK6].SetPosition(1024, 119);

    m_asprTitle[UIM_TS_BACK7].Create(512, 223, BITMAP_TITLE + 11, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK7].SetPosition(0, 512 + 119);

    m_asprTitle[UIM_TS_BACK8].Create(512, 223, BITMAP_TITLE + 12, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK8].SetPosition(512, 512 + 119);

    m_asprTitle[UIM_TS_BACK9].Create(256, 223, BITMAP_TITLE + 13, 0, NULL, 0, 0, false,
                                     SPR_SIZING_DATUMS_LT, _fScaleXTemp, _fScaleYTemp);
    m_asprTitle[UIM_TS_BACK9].SetPosition(1024, 512 + 119);

    m_asprTitle[UIM_TS_121518].Create(256, 206, BITMAP_TITLE + 3, 0, NULL, 0, 0, false,
                                      SPR_SIZING_DATUMS_LT, fScaleX, fScaleY);
    m_asprTitle[UIM_TS_121518].SetPosition(544, 60);

    m_pgbLoding = new CGaugeBar(sessionKeeper_);

    RECT rc = {0, 0, 656, 15};
    m_pgbLoding->Create(4, 15, BITMAP_TITLE + 5, &rc, 0, 0, -1, true, fScaleX, fScaleY);

    m_pgbLoding->SetPosition(72, 540);
    for (int i = 0; i < UIM_TS_MAX; ++i)
    {
        m_asprTitle[i].Show();
    }
    m_pgbLoding->Show();
    m_asprTitle[UIM_TS_121518].Show(false);
    m_nScene = UIM_SCENE_TITLE;
}

void CUIMng::ReleaseTitleSceneUI()
{
    titleSprites_.reset();
    m_asprTitle = nullptr;
    SAFE_DELETE(m_pgbLoding);

    m_nScene = UIM_SCENE_NONE;
}

void CUIMng::Create()
{
    m_bCursorOnUI = false;
    m_bBlockCharMove = false;
    m_bWinActive = false;
    escapeKeyHeld_ = false;
    m_nScene = UIM_SCENE_NONE;

    return;
}

void CUIMng::RemoveWinList()
{
    CWin *pWin;
    while (m_WinList.GetCount())
    {
        pWin = (CWin *)m_WinList.RemoveHead();
        pWin->Release();
    }
}

void CUIMng::Release()
{
    RemoveWinList();

    if (m_CharInfoBalloonMng != nullptr)
    {
        m_CharInfoBalloonMng->Release();
    }

    m_nScene = UIM_SCENE_NONE;
    escapeKeyHeld_ = false;
}

void CUIMng::CreateLoginScene()
{
    RemoveWinList();

    m_CharInfoBalloonMng->Release();

    const unsigned int modernUiWidth = renderer_.ModernUiLogicalViewportWidth();
    const unsigned int modernUiHeight = renderer_.ModernUiLogicalViewportHeight();
    const int loginViewportWidth =
        static_cast<int>(modernUiWidth == 0 ? WindowWidth : modernUiWidth);
    const int loginViewportHeight =
        static_cast<int>(modernUiHeight == 0 ? WindowHeight : modernUiHeight);

    m_MsgWin->Create();
    m_WinList.AddHead(m_MsgWin.get());
    m_MsgWin->SetPosition((loginViewportWidth - CMsgWin::Width()) / 2,
                          (loginViewportHeight - CMsgWin::Height()) / 2);

    m_SysMenuWin->Create();
    m_WinList.AddHead(m_SysMenuWin.get());

    m_LoginMainWin->Create();
    m_WinList.AddHead(m_LoginMainWin.get());

    m_LoginMainWin->SetPosition(
        UI::Modern::PC::Login::RmlLoginSceneButtons::HorizontalMargin(),
        UI::Modern::PC::Login::RmlLoginSceneButtons::ButtonY(loginViewportHeight));

    m_ServerSelWin->Create();
    m_WinList.AddHead(m_ServerSelWin.get());
    m_ServerSelWin->SetPosition((static_cast<int>(WindowWidth) - m_ServerSelWin->GetWidth()) / 2,
                                (static_cast<int>(WindowHeight) - m_ServerSelWin->GetHeight()) / 2);

    m_LoginWin->Create();
    m_WinList.AddHead(m_LoginWin.get());
    m_LoginWin->SetPosition(UI::Modern::PC::Login::RmlLoginPanel::LeftFor(loginViewportWidth),
                            UI::Modern::PC::Login::RmlLoginPanel::TopFor(loginViewportHeight));

    m_CreditWin->Create();
    m_WinList.AddHead(m_CreditWin.get());

    m_bSysMenuWinShow = false;
    escapeKeyHeld_ = IsPress(VK_ESCAPE) || IsRepeat(VK_ESCAPE);
    m_nScene = UIM_SCENE_LOGIN;
}

void CUIMng::CreateCharacterScene()
{
    RemoveWinList();

    m_CharInfoBalloonMng->Create();

    const unsigned int modernUiWidth = renderer_.ModernUiLogicalViewportWidth();
    const unsigned int modernUiHeight = renderer_.ModernUiLogicalViewportHeight();
    const int characterViewportWidth =
        static_cast<int>(modernUiWidth == 0 ? WindowWidth : modernUiWidth);
    const int characterViewportHeight =
        static_cast<int>(modernUiHeight == 0 ? WindowHeight : modernUiHeight);

    m_MsgWin->Create();
    m_WinList.AddHead(m_MsgWin.get());
    m_MsgWin->SetPosition((characterViewportWidth - CMsgWin::Width()) / 2,
                          (characterViewportHeight - CMsgWin::Height()) / 2);

    m_ServerMsgWin->Create();
    m_WinList.AddHead(m_ServerMsgWin.get());
    m_ServerMsgWin->SetPosition(
        UI::Modern::PC::ServerMessage::RmlServerMessagePanel::Left(),
        UI::Modern::PC::ServerMessage::RmlServerMessagePanel::Top(WindowHeight));

    m_SysMenuWin->Create();
    m_WinList.AddHead(m_SysMenuWin.get());

    m_CharSelMainWin->Create();
    m_WinList.AddHead(m_CharSelMainWin.get());
    const int nBaseY = int(567.0f / 600.0f * static_cast<float>(WindowHeight));
    m_CharSelMainWin->SetPosition(22, nBaseY - m_CharSelMainWin->GetHeight() - 11);

    m_CharMakeWin->Create();
    m_WinList.AddHead(m_CharMakeWin.get());

    ShowWin(m_CharSelMainWin.get());

    // UpdateDisplay() auto-shows the character creation window when the account
    // has no live character (see CCharSelMainWin::UpdateDisplay). It has to run
    // *after* ShowWin(m_CharSelMainWin) so the creation window ends up on top
    // and active: CWin::Update only calls UpdateWhileActive() for the active
    // window, and that is where the Rml creation panel consumes its clicks.
    m_CharSelMainWin->UpdateDisplay();
    m_CharInfoBalloonMng->UpdateDisplay();

    m_bSysMenuWinShow = false;
    escapeKeyHeld_ = IsPress(VK_ESCAPE) || IsRepeat(VK_ESCAPE);
    m_nScene = UIM_SCENE_CHARACTER;
}

void CUIMng::CreateMainScene()
{
    RemoveWinList();

    m_CharInfoBalloonMng->Release();

    m_nScene = UIM_SCENE_MAIN;
}

void CUIMng::RepositionSceneUI()
{
    // A lightweight SetPosition sweep isn't enough: CSprite caches
    // m_fScrHeight = WindowHeight at Create() time and uses it for the
    // Y-flipped coordinate math in SetPosition(). When the window resizes,
    // every sprite's cached screen height is stale, so a pure SetPosition
    // call lands the windows in the wrong place.
    // The only clean way to refresh that cache is to re-Create the sprites,
    // which is exactly what the scene's Create*Scene() function does. But
    // that also resets each window's m_bShow flag, so we snapshot the
    // current visibility here and restore it right after.
    if (m_nScene == UIM_SCENE_LOGIN)
    {
        const bool wasShown_MsgWin = m_MsgWin->IsShow();
        const bool wasShown_SysMenuWin = m_SysMenuWin->IsShow();
        const bool wasShown_LoginMainWin = m_LoginMainWin->IsShow();
        const bool wasShown_ServerSelWin = m_ServerSelWin->IsShow();
        const bool wasShown_LoginWin = m_LoginWin->IsShow();
        const bool wasShown_CreditWin = m_CreditWin->IsShow();

        CreateLoginScene();

        // Restore visibility BEFORE re-populating dynamic windows: child
        // elements like server/group buttons read `CWin::m_bShow` of their
        // parent when `UpdateDisplay()` decides which sub-elements to show.
        // If the parent is still hidden at that moment, nothing renders.
        if (wasShown_MsgWin)
            ShowWin(m_MsgWin.get());
        if (wasShown_SysMenuWin)
            ShowWin(m_SysMenuWin.get());
        if (wasShown_LoginMainWin)
            ShowWin(m_LoginMainWin.get());
        if (wasShown_ServerSelWin)
            ShowWin(m_ServerSelWin.get());
        if (wasShown_LoginWin)
            ShowWin(m_LoginWin.get());
        if (wasShown_CreditWin)
            ShowWin(m_CreditWin.get());

        // Re-populate the server / server-group buttons from the existing
        // network-side data. Create() clears the button labels, so without
        // this the server list and groups render empty after a resolution
        // change.
        m_ServerSelWin->UpdateDisplay();
    }
    else if (m_nScene == UIM_SCENE_CHARACTER)
    {
        // CreateCharacterScene() ends with an explicit ShowWin(&m_CharSelMainWin)
        // so visibility of the main panel is already preserved. Other character-
        // scene windows (msg box, server msg, char make) are shown on demand
        // by game events, matching the fresh-scene state.
        CreateCharacterScene();
    }
    // MainScene uses the new UI system which resizes itself; nothing to do.
}

CWin *CUIMng::SetActiveWin(CWin *pWin)
{
    CWin *pBeforeActWin = (CWin *)m_WinList.GetHead();

    if (pBeforeActWin == NULL)
        return NULL;

    if (pBeforeActWin->IsActive())
        pBeforeActWin->Active(FALSE);
    else
        pBeforeActWin = NULL;

    if (pWin->IsShow())
    {
        if (!m_WinList.RemoveAt(m_WinList.Find(pWin)))
            return NULL;

        m_bWinActive = true;
        m_WinList.AddHead(pWin);
    }

    return pBeforeActWin;
}

void CUIMng::ShowWin(CWin *pWin)
{
    pWin->Show(TRUE);
    SetActiveWin(pWin);
}

void CUIMng::HideWin(CWin *pWin)
{
    if (!m_WinList.RemoveAt(m_WinList.Find(pWin)))
        return;

    pWin->Show(FALSE);
    pWin->Active(FALSE);
    m_WinList.AddTail(pWin);

    pWin = (CWin *)m_WinList.GetHead();
    if (pWin->IsShow())
        m_bWinActive = true;
}

void CUIMng::CheckDockWin()
{
    NODE *position = m_WinList.GetHeadPosition();
    if (NULL == position)
        return;

    CWin *pMovWin = (CWin *)m_WinList.GetNext(position);

    if (pMovWin->GetState() != WS_MOVE)
        return;

    pMovWin->SetDocking(false);

    RECT rcMovWin = {pMovWin->GetTempXPos(), pMovWin->GetTempYPos(),
                     pMovWin->GetTempXPos() + pMovWin->GetWidth(),
                     pMovWin->GetTempYPos() + pMovWin->GetHeight()};

    RECT rcDock[4] = {{rcMovWin.left - DOCK_EXTENT, rcMovWin.top - DOCK_EXTENT,
                       rcMovWin.left + DOCK_EXTENT, rcMovWin.top + DOCK_EXTENT},
                      {rcMovWin.right - DOCK_EXTENT, rcMovWin.top - DOCK_EXTENT,
                       rcMovWin.right + DOCK_EXTENT, rcMovWin.top + DOCK_EXTENT},
                      {rcMovWin.left - DOCK_EXTENT, rcMovWin.bottom - DOCK_EXTENT,
                       rcMovWin.left + DOCK_EXTENT, rcMovWin.bottom + DOCK_EXTENT},
                      {rcMovWin.right - DOCK_EXTENT, rcMovWin.bottom - DOCK_EXTENT,
                       rcMovWin.right + DOCK_EXTENT, rcMovWin.bottom + DOCK_EXTENT}};

    POINT pt[4] = {{0, 0},
                   {static_cast<LONG>(WindowWidth), 0},
                   {0, static_cast<LONG>(WindowHeight)},
                   {static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight)}};

    if (::PtInRect(&rcDock[0], pt[0]))
    {
        pMovWin->SetPosition(pt[0].x, pt[0].y);
        pMovWin->SetDocking(true);
    }
    else if (::PtInRect(&rcDock[1], pt[1]))
    {
        pMovWin->SetPosition(pt[1].x - pMovWin->GetWidth(), pt[1].y);
        pMovWin->SetDocking(true);
    }
    else if (::PtInRect(&rcDock[2], pt[2]))
    {
        pMovWin->SetPosition(pt[2].x, pt[2].y - pMovWin->GetHeight());
        pMovWin->SetDocking(true);
    }
    else if (::PtInRect(&rcDock[3], pt[3]))
    {
        pMovWin->SetPosition(pt[3].x - pMovWin->GetWidth(), pt[3].y - pMovWin->GetHeight());
        pMovWin->SetDocking(true);
    }
    else if (rcDock[0].top < 0 && rcDock[0].bottom > 0)
    {
        pMovWin->SetPosition(rcMovWin.left, 0);
        pMovWin->SetDocking(true);
    }
    else if (rcDock[2].top < pt[2].y && rcDock[2].bottom > pt[2].y)
    {
        pMovWin->SetPosition(rcMovWin.left, pt[2].y - pMovWin->GetHeight());
        pMovWin->SetDocking(true);
    }
    else if (rcDock[0].left < 0 && rcDock[0].right > 0)
    {
        pMovWin->SetPosition(0, rcMovWin.top);
        pMovWin->SetDocking(true);
    }
    else if (rcDock[1].left < pt[1].x && rcDock[1].right > pt[1].x)
    {
        pMovWin->SetPosition(pt[1].x - pMovWin->GetWidth(), rcMovWin.top);
        pMovWin->SetDocking(true);
    }

    BOOL bEdgeDocking = FALSE;
    int i, j, nXCoord, nYCoord;
    CWin *pWin;

    while (position)
    {
        pWin = (CWin *)m_WinList.GetNext(position);
        if (!pWin->IsShow())
            continue;

        pt[0].x = pWin->GetXPos();
        pt[0].y = pWin->GetYPos();
        pt[1].x = pWin->GetXPos() + pWin->GetWidth();
        pt[1].y = pt[0].y;
        pt[2].x = pt[0].x;
        pt[2].y = pWin->GetYPos() + pWin->GetHeight();
        pt[3].x = pt[1].x;
        pt[3].y = pt[2].y;

        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                if (i != j && ::PtInRect(&rcDock[i], pt[j]))
                {
                    bEdgeDocking = TRUE;
                    goto DOCKING;
                }
            }
        }

        if (pt[0].x < rcDock[1].left && pt[1].x > rcDock[0].right)
        {
            nXCoord = rcMovWin.left;
            if (pt[2].y > rcDock[0].top && pt[2].y < rcDock[0].bottom)
            {
                if (SetDockWinPosition(pMovWin, nXCoord, pt[2].y))
                    continue;
            }
            else if (pt[0].y > rcDock[2].top && pt[0].y < rcDock[2].bottom)
            {
                if (SetDockWinPosition(pMovWin, nXCoord, pt[0].y - pMovWin->GetHeight()))
                    continue;
            }
        }
        else if (pt[0].y < rcDock[2].top && pt[2].y > rcDock[0].bottom)
        {
            nYCoord = rcMovWin.top;
            if (pt[1].x > rcDock[0].left && pt[1].x < rcDock[0].right)
            {
                if (SetDockWinPosition(pMovWin, pt[1].x, nYCoord))
                    continue;
            }
            else if (pt[0].x > rcDock[1].left && pt[0].x < rcDock[1].right)
            {
                if (SetDockWinPosition(pMovWin, pt[0].x - pMovWin->GetWidth(), nYCoord))
                    continue;
            }
        }
    }

DOCKING:
    if (bEdgeDocking)
    {
        switch (j)
        {
        case 0:
            switch (i)
            {
            case 1:
                nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos();
                break;
            case 2:
                nXCoord = pWin->GetXPos();
                nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
                break;
            case 3:
                nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
            }
            break;

        case 1:
            switch (i)
            {
            case 0:
                nXCoord = pWin->GetXPos() + pWin->GetWidth();
                nYCoord = pWin->GetYPos();
                break;
            case 2:
                nXCoord = pWin->GetXPos() + pWin->GetWidth();
                nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
                break;
            case 3:
                nXCoord = pWin->GetXPos() + pWin->GetWidth() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos() - pMovWin->GetHeight();
            }
            break;

        case 2:
            switch (i)
            {
            case 0:
                nXCoord = pWin->GetXPos();
                nYCoord = pWin->GetYPos() + pWin->GetHeight();
                break;
            case 1:
                nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos() + pWin->GetHeight();
                break;
            case 3:
                nXCoord = pWin->GetXPos() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos() + pWin->GetHeight() - pMovWin->GetHeight();
            }
            break;

        case 3:
            switch (i)
            {
            case 0:
                nXCoord = pWin->GetXPos() + pWin->GetWidth();
                nYCoord = pWin->GetYPos() + pWin->GetHeight();
                break;
            case 1:
                nXCoord = pWin->GetXPos() + pWin->GetWidth() - pMovWin->GetWidth();
                nYCoord = pWin->GetYPos() + pWin->GetHeight();
                break;
            case 2:
                nXCoord = pWin->GetXPos() + pWin->GetWidth();
                nYCoord = pWin->GetYPos() + pWin->GetHeight() - pMovWin->GetHeight();
            }
        }
        SetDockWinPosition(pMovWin, nXCoord, nYCoord);
    }
}

bool CUIMng::SetDockWinPosition(CWin *pMoveWin, int nDockX, int nDockY)
{
    RECT rcDummy;
    RECT rcScreen = {0, 0, static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight)};
    RECT rcMoveWin = {nDockX, nDockY, nDockX + pMoveWin->GetWidth(),
                      nDockY + pMoveWin->GetHeight()};

    if (::IntersectRect(&rcDummy, &rcScreen, &rcMoveWin))
    {
        pMoveWin->SetPosition(nDockX, nDockY);
        pMoveWin->SetDocking(true);
        return true;
    }

    return false;
}

void CUIMng::Update(double dDeltaTick)
{
    if (UIM_SCENE_NONE == m_nScene || m_WinList.IsEmpty())
        return;

    if (m_bWinActive)
    {
        CWin *pWin = (CWin *)m_WinList.GetHead();
        if (pWin->IsShow())
        {
            pWin->Active(true);
            m_bWinActive = false;
        }
    }

    CInput &rInput = input_;

    // Scene updates may observe the same key snapshot more than once. Keep the
    // menu action edge-triggered so holding ESC cannot show/hide it repeatedly.
    const bool escapeDown = IsPress(VK_ESCAPE) || IsRepeat(VK_ESCAPE);
    const bool escapePressed = escapeDown && !escapeKeyHeld_;
    escapeKeyHeld_ = escapeDown;
    if (escapePressed)
    {
        if (SceneFlag == LOG_IN_SCENE || SceneFlag == CHARACTER_SCENE)
        {
            if (m_SysMenuWin->IsShow())
            {
                HideWin(m_SysMenuWin.get());
            }
            else if (!m_MsgWin->IsShow() && !m_LoginWin->IsShow() && !m_CreditWin->IsShow() &&
                     !m_CharMakeWin->IsShow())
            {
                m_SysMenuWin->PlayBuffer(SOUND_CLICK01);
                ShowWin(m_SysMenuWin.get());
            }
        }
    }

    CWin *pWin;
    NODE *position;

    m_bCursorOnUI = false;

    if (MouseLButtonPush)
    {
        bool bWinClick = false;
        position = m_WinList.GetHeadPosition();
        while (position)
        {
            pWin = (CWin *)m_WinList.GetNext(position);

            if (pWin->CursorInWin(WA_ALL))
            {
                SetActiveWin(pWin);
                bWinClick = true;
                break;
            }
        }

        if (!bWinClick)
        {
            pWin = (CWin *)m_WinList.GetHead();
            pWin->Active(false);
        }
    }
    else if (MouseLButtonPop)
    {
        m_bBlockCharMove = false;
    }
    int nlist = m_WinList.GetCount();
    std::vector<CWin *> apTempWin(nlist);

    position = m_WinList.GetHeadPosition();
    for (int i = 0; i < nlist; ++i)
    {
        apTempWin[i] = (CWin *)m_WinList.GetNext(position);
        apTempWin[i]->ActiveBtns(false);
    }

    position = m_WinList.GetHeadPosition();
    while (position)
    {
        pWin = (CWin *)m_WinList.GetNext(position);
        if (pWin->CursorInWin(WA_ALL))
        {
            pWin->ActiveBtns(true);
            break;
        }
    }

    for (int i = 0; i < nlist; ++i)
    {
        apTempWin[i]->Update(dDeltaTick);
    }

    //	CheckKey();
    CheckDockWin();

    position = m_WinList.GetHeadPosition();
    while (position)
    {
        pWin = (CWin *)m_WinList.GetNext(position);

        switch (pWin->GetState())
        {
        case WS_ETC:
            m_bCursorOnUI = true;
            break;

        case WS_MOVE:
            //			eCursorActType = CURSOR_M;
            m_bCursorOnUI = true;
            break;

        case WS_EXTEND_UP:
            //			eCursorActType = CURSOR_V;
            m_bCursorOnUI = true;
            break;

        case WS_EXTEND_DN:
            //			eCursorActType = CURSOR_V;
            m_bCursorOnUI = true;
            break;
        }

        if (m_bCursorOnUI)
            break;

        if (pWin->CursorInWin(WA_ALL))
        {
            m_bCursorOnUI = true;
            break;
        }
    }
}

bool CUIMng::ProcessModernUiInput(const SessionInputEvent &event)
{
    if (m_nScene == UIM_SCENE_MAIN &&
        sessionKeeper_.MessageBoxManagerObject().ProcessModernUiInput(event))
    {
        return true;
    }
    if (m_nScene == UIM_SCENE_MAIN && g_pNewUISystem && g_pQuickCommand &&
        g_pQuickCommand->ProcessModernUiInput(event))
        return true;
    if (m_nScene == UIM_SCENE_MAIN && g_pNewUISystem &&
        g_pNewUISystem->GetUI_pNewUnitedMarketPlaceWindow() &&
        g_pNewUISystem->GetUI_pNewUnitedMarketPlaceWindow()->ProcessModernUiInput(event))
        return true;
    if (const auto processed = ProcessModernInventoryPanelsInput(event))
        return *processed;
    if (g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION) &&
        g_pOption != nullptr)
    {
        return g_pOption->ProcessModernUiInput(event);
    }
    if (m_nScene == UIM_SCENE_MAIN && g_pChatInputBox != nullptr)
    {
        if (g_pChatInputBox->ProcessModernUiInput(event))
            return true;
    }
    if (m_MsgWin != nullptr && m_MsgWin->IsShow())
    {
        return m_MsgWin->ProcessModernUiInput(event);
    }
    if (m_SysMenuWin != nullptr && m_SysMenuWin->IsShow())
    {
        return m_SysMenuWin->ProcessModernUiInput(event);
    }
    if (m_nScene == UIM_SCENE_CHARACTER && m_CharMakeWin && m_CharMakeWin->IsShow())
        return m_CharMakeWin->ProcessModernUiInput(event);
    if (m_nScene != UIM_SCENE_LOGIN)
    {
        return false;
    }
    if (m_LoginWin != nullptr && m_LoginWin->IsShow())
    {
        return m_LoginWin->ProcessModernUiInput(event);
    }
    return m_LoginMainWin != nullptr && m_LoginMainWin->ProcessModernUiInput(event);
}

namespace
{
bool ModernPanelIsAbove(SEASON3B::CNewUIObj *left, SEASON3B::CNewUIObj *right)
{
    if (!left)
        return false;
    if (!right)
        return true;
    return UI::Modern::MigratedUiRenderLayers::IsBefore(
        right->GetLayerDepth(), right->GetDynamicLayerOrder(), left->GetLayerDepth(),
        left->GetDynamicLayerOrder());
}
void OrderModernPanels(auto &order, const auto &panels)
{
    std::sort(order.begin(), order.end(), [&](auto left, auto right) {
        return ModernPanelIsAbove(panels[left], panels[right]);
    });
}
} // namespace

std::optional<bool> CUIMng::ProcessModernInventoryPanelsInput(const SessionInputEvent &event)
{
    if (m_nScene != UIM_SCENE_MAIN || !g_pNewUISystem)
        return std::nullopt;
    enum Target
    {
        Inventory,
        Extension,
        Seller,
        Buyer,
        Shop,
        Storage,
        StorageExt,
        Trade,
        Mix,
        Lucky,
        Character,
        Pet,
        Helper,
        Command,
        Friend,
        Party,
        WindowMenu,
        Help,
        NpcDialogue,
        Durability,
        ItemExplanation,
        SetExplanation,
        NpcQuest,
        QuestProgress,
        QuestProgressEtc,
        QuestJournal,
        GuildCreate,
        GuildInfo,
        Buff,
        MasterTree,
        Gens,
        DuelWatch,
        GoldArcher,
        Kanturu,
        BloodCastle,
        Gatekeeper,
        EmpireGuardian,
        Temple,
        TempleResult,
        Count
    };
    const std::array<SEASON3B::CNewUIObj *, Count> panels{g_pMyInventory,
                                                          g_pMyInventoryExt,
                                                          g_pMyShopInventory,
                                                          g_pPurchaseShopInventory,
                                                          g_pNPCShop,
                                                          g_pStorageInventory,
                                                          g_pStorageInventoryExt,
                                                          g_pTrade,
                                                          g_pMixInventory,
                                                          g_pLuckyItemWnd,
                                                          g_pCharacterInfoWindow,
                                                          g_pPetInfoWindow,
                                                          g_pNewUIMuHelper,
                                                          g_pCommandWindow,
                                                          g_pNewUISystem->GetUI_NewFriendWindow(),
                                                          g_pPartyListWindow,
                                                          g_pWindowMenu,
                                                          g_pHelp,
                                                          g_pNPCDialogue,
                                                          g_pItemEnduranceInfo,
                                                          g_pItemExplanation,
                                                          g_pSetItemExplanation,
                                                          g_pNPCQuest,
                                                          g_pQuestProgress,
                                                          g_pQuestProgressByEtc,
                                                          g_pMyQuestInfoWindow,
                                                          g_pGuildMakeWindow,
                                                          g_pGuildInfoWindow,
                                                          g_pBuffWindow,
                                                          g_pMasterLevelInterface,
#ifdef PBG_ADD_GENSRANKING
                                                          g_pNewUIGensRanking,
#else
                                                          nullptr,
#endif
                                                          g_pDuelWatchWindow,
                                                          g_pGoldBowmanInterface,
                                                          g_pKanturu2ndEnterNpc,
                                                          g_pEnterBloodCastle,
                                                          g_pGatemanWindow,
                                                          g_pEmpireGuardianNPC,
                                                          g_pCursedTempleWindow,
                                                          g_pCursedTempleResultWindow};
    std::array<Target, Count> order{Inventory,
                                    Extension,
                                    Seller,
                                    Buyer,
                                    Shop,
                                    Storage,
                                    StorageExt,
                                    Trade,
                                    Mix,
                                    Lucky,
                                    Character,
                                    Pet,
                                    Helper,
                                    Command,
                                    Friend,
                                    Party,
                                    WindowMenu,
                                    Help,
                                    NpcDialogue,
                                    Durability,
                                    ItemExplanation,
                                    SetExplanation,
                                    NpcQuest,
                                    QuestProgress,
                                    QuestProgressEtc,
                                    QuestJournal,
                                    GuildCreate,
                                    GuildInfo,
                                    Buff,
                                    MasterTree,
                                    Gens,
                                    DuelWatch,
                                    GoldArcher,
                                    Kanturu,
                                    BloodCastle,
                                    Gatekeeper,
                                    EmpireGuardian,
                                    Temple,
                                    TempleResult};
    OrderModernPanels(order, panels);
    for (const auto target : order)
    {
        if (!panels[target] || !panels[target]->IsVisible())
            continue;
        std::optional<bool> handled;
        switch (target)
        {
        case Inventory:
            handled = g_pMyInventory->ProcessModernUiInput(event);
            break;
        case Extension:
            handled = g_pMyInventoryExt->ProcessModernUiInput(event);
            break;
        case Seller:
            handled = g_pMyShopInventory->ProcessModernUiInput(event);
            break;
        case Buyer:
            handled = g_pPurchaseShopInventory->ProcessModernUiInput(event);
            break;
        case Shop:
            handled = g_pNPCShop->ProcessModernUiInput(event);
            break;
        case Storage:
            handled = g_pStorageInventory->ProcessModernUiInput(event);
            break;
        case StorageExt:
            handled = g_pStorageInventoryExt->ProcessModernUiInput(event);
            break;
        case Trade:
            handled = g_pTrade->ProcessModernUiInput(event);
            break;
        case Mix:
            handled = g_pMixInventory->ProcessModernUiInput(event);
            break;
        case Lucky:
            handled = g_pLuckyItemWnd->ProcessModernUiInput(event);
            break;
        case Character:
            if (g_pCharacterInfoWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Pet:
            if (g_pPetInfoWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Helper:
            if (g_pNewUIMuHelper->ProcessModernUiInput(event))
                handled = true;
            break;
        case Command:
            if (g_pCommandWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Friend:
            if (g_pNewUISystem->GetUI_NewFriendWindow()->ProcessModernUiInput(event))
                handled = true;
            break;
        case Party:
            if (g_pPartyListWindow->OwnsModernPointer(event))
                handled = false;
            break;
        case WindowMenu:
            if (g_pWindowMenu->ProcessModernUiInput(event))
                handled = true;
            break;
        case Gens:
#ifdef PBG_ADD_GENSRANKING
            if (g_pNewUIGensRanking->ProcessModernUiInput(event))
                handled = true;
#endif
            break;
        case Count:
            break;
        case TempleResult:
            if (g_pCursedTempleResultWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Temple:
            if (g_pCursedTempleWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case EmpireGuardian:
            if (g_pEmpireGuardianNPC->ProcessModernUiInput(event))
                handled = true;
            break;
        case Gatekeeper:
            if (g_pGatemanWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case BloodCastle:
            if (g_pEnterBloodCastle->ProcessModernUiInput(event))
                handled = true;
            break;
        case Kanturu:
            if (g_pKanturu2ndEnterNpc->ProcessModernUiInput(event))
                handled = true;
            break;
        case GoldArcher:
            if (g_pGoldBowmanInterface->ProcessModernUiInput(event))
                handled = true;
            break;
        case DuelWatch:
            if (g_pDuelWatchWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Help:
            if (g_pHelp->ProcessModernUiInput(event))
                handled = true;
            break;
        case ItemExplanation:
            if (g_pItemExplanation->ProcessModernUiInput(event))
                handled = true;
            break;
        case SetExplanation:
            if (g_pSetItemExplanation->ProcessModernUiInput(event))
                handled = true;
            break;
        case MasterTree:
            if (g_pMasterLevelInterface->ProcessModernUiInput(event))
                handled = true;
            break;
        case Buff:
            if (g_pBuffWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case Durability:
            if (g_pItemEnduranceInfo->ProcessModernUiInput(event))
                handled = true;
            break;
        case GuildInfo:
            if (g_pGuildInfoWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case GuildCreate:
            if (g_pGuildMakeWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case QuestJournal:
            if (g_pMyQuestInfoWindow->ProcessModernUiInput(event))
                handled = true;
            break;
        case QuestProgress:
            if (g_pQuestProgress->ProcessModernUiInput(event))
                handled = true;
            break;
        case QuestProgressEtc:
            if (g_pQuestProgressByEtc->ProcessModernUiInput(event))
                handled = true;
            break;
        case NpcQuest:
            if (g_pNPCQuest->ProcessModernUiInput(event))
                handled = true;
            break;
        case NpcDialogue:
            if (g_pNPCDialogue->ProcessModernUiInput(event))
                handled = true;
            break;
        }
        if (handled.has_value())
            return handled;
    }
    return std::nullopt;
}

std::optional<bool> CUIMng::ProcessModernCharacterPanelsInput(const SessionInputEvent &event)
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_PET) && g_pPetInfoWindow != nullptr)
    {
        return g_pPetInfoWindow->ProcessModernUiInput(event);
    }
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER) &&
        g_pCharacterInfoWindow != nullptr)
    {
        return g_pCharacterInfoWindow->ProcessModernUiInput(event);
    }
    return std::nullopt;
}

bool CUIMng::IsCursorOnUI() const
{
    return m_bCursorOnUI ||
           (g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION));
}

std::optional<UI::Modern::RmlTextInputArea> CUIMng::ModernTextInputArea() const
{
    focusedModernUiObject_ = nullptr;
    if (m_MsgWin != nullptr && m_MsgWin->IsShow())
    {
        return m_MsgWin->ModernTextInputArea();
    }
    if (m_nScene == UIM_SCENE_MAIN)
    {
        return ModernMainTextInputArea();
    }
    if (m_nScene == UIM_SCENE_CHARACTER && m_CharMakeWin && m_CharMakeWin->IsShow())
        return m_CharMakeWin->ModernTextInputArea();
    return m_nScene == UIM_SCENE_LOGIN && m_LoginWin != nullptr ? m_LoginWin->ModernTextInputArea()
                                                                : std::nullopt;
}

std::optional<UI::Modern::RmlTextInputArea> CUIMng::ModernMainTextInputArea() const
{
    auto &messageBoxes = sessionKeeper_.MessageBoxManagerObject();
    if (auto area = messageBoxes.ModernTextInputArea())
    {
        focusedModernUiObject_ = &messageBoxes;
        return area;
    }
    if (g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND))
    {
        if (auto *friendWindow = g_pNewUISystem->GetUI_NewFriendWindow())
        {
            if (auto area = friendWindow->ModernTextInputArea())
            {
                focusedModernUiObject_ = friendWindow;
                return area;
            }
        }
    }
    if (g_pNewUISystem && g_pGuildMakeWindow && g_pGuildMakeWindow->IsVisible())
        if (auto area = g_pGuildMakeWindow->ModernTextInputArea())
        {
            focusedModernUiObject_ = g_pGuildMakeWindow;
            return area;
        }
    if (auto area = ModernEventTextInputArea())
        return area;
    if (g_pNewUIMuHelper != nullptr && g_pNewUIMuHelper->IsVisible())
    {
        if (auto area = g_pNewUIMuHelper->ModernTextInputArea())
        {
            focusedModernUiObject_ = g_pNewUIMuHelper;
            return area;
        }
    }
    if (g_pMyShopInventory != nullptr && g_pMyShopInventory->IsVisible())
    {
        if (auto area = g_pMyShopInventory->ModernTextInputArea())
        {
            focusedModernUiObject_ = g_pMyShopInventory;
            return area;
        }
    }
    if (g_pChatInputBox == nullptr)
        return std::nullopt;
    auto area = g_pChatInputBox->ModernTextInputArea();
    if (area.has_value())
        focusedModernUiObject_ = g_pChatInputBox;
    return area;
}

std::optional<UI::Modern::RmlTextInputArea> CUIMng::ModernEventTextInputArea() const
{
    if (!g_pNewUISystem)
        return std::nullopt;
    auto *archer = g_pNewUISystem->GetUI_pNewGoldBowman();
    if (!archer || !archer->IsVisible())
        return std::nullopt;
    auto area = archer->ModernTextInputArea();
    if (area)
        focusedModernUiObject_ = archer;
    return area;
}

SEASON3B::CNewUIObj *CUIMng::FocusedModernUiObject() const noexcept
{
    return focusedModernUiObject_;
}

void CUIMng::PopUpMsgWin(int nMsgCode, wchar_t *pszMsg)
{
    if (UIM_SCENE_NONE == m_nScene || UIM_SCENE_TITLE == m_nScene || UIM_SCENE_LOADING == m_nScene)
        return;

    if (UIM_SCENE_MAIN == m_nScene)
        return;

    m_MsgWin->PopUp(nMsgCode, pszMsg);
}

void CUIMng::AddServerMsg(wchar_t *pszMsg)
{
    if (UIM_SCENE_CHARACTER != m_nScene)
        return;

    m_ServerMsgWin->AddMsg(pszMsg);
}

bool SessionUiUnit::CreateOkMessageBox(const std::wstring &strMsg, DWORD dwColor, float fPriority)
{
    SEASON3B::CNewUIMessageBoxFactory::TContainer<SEASON3B::CNewUICommonMessageBox> container(
        SessionOrigin());
    SEASON3B::CNewUICommonMessageBox *pMsgBox = g_MessageBox.NewMessageBox(container);
    if (pMsgBox)
    {
        return pMsgBox->Create(SEASON3B::MSGBOX_COMMON_TYPE_OK, strMsg, dwColor);
    }
    return false;
}

bool SessionLegacyCalls::CreateOkMessageBox(const std::wstring &message, DWORD color,
                                            float priority) const
{
    return sessionKeeper_.Ui()->CreateOkMessageBox(message, color, priority);
}

int SessionUiUnit::IsPurchaseShop()
{
    if (g_pMyShopInventory->IsVisible())
    {
        return 1;
    }
    else if (g_pPurchaseShopInventory->IsVisible())
    {
        return 2;
    }

    return -1;
}

int SessionLegacyCalls::IsPurchaseShop()
{
    return sessionKeeper_.Ui()->IsPurchaseShop();
}

bool SessionUiUnit::CheckMouseIn(int x, int y, int width, int height) const
{
    return MouseX >= x && MouseX < x + width && MouseY >= y && MouseY < y + height;
}

bool SessionLegacyCalls::CheckMouseIn(int x, int y, int width, int height) const
{
    return sessionKeeper_.Ui()->CheckMouseIn(x, y, width, height);
}

SEASON3B::CNewKeyInput::CNewKeyInput(ApplicationKeeper &keeper) noexcept
    : ApplicationLegacyCalls(keeper), m_pInputInfo(keeper.PlatformStorageRef().newKeyInputStates),
      g_bEnterPressed(keeper.PlatformStorageRef().enterPressed)
{
    Init();
    (void)applicationKeeper_.RegisterNewKeyInput(*this);
}

SEASON3B::CNewKeyInput::~CNewKeyInput()
{
}

void SEASON3B::CNewKeyInput::Init()
{
    memset(&m_pInputInfo, 0, sizeof(m_pInputInfo));
}

void SEASON3B::CNewKeyInput::ScanAsyncKeyState()
{
#ifdef ASG_FIX_ACTIVATE_APP_INPUT
    if (SDL_GetKeyboardFocus() == nullptr)
        return;
#endif // ASG_FIX_ACTIVATE_APP_INPUT

    for (int key = 0; key < 256; key++)
    {
        if (IsKeyDown(key))
        {
            if (m_pInputInfo[key] == KEY_NONE || m_pInputInfo[key] == KEY_RELEASE)
            {
                // press event (key was up before but down now)
                m_pInputInfo[key] = KEY_PRESS;
            }
            else if (m_pInputInfo[key] == KEY_PRESS)
            {
                // drag event (key is still down)
                m_pInputInfo[key] = KEY_REPEAT;
            }
        }
        else // Key is not currently pressed
        {
            if (m_pInputInfo[key] == KEY_REPEAT || m_pInputInfo[key] == KEY_PRESS)
            {
                // release event (key was down before but up now)
                m_pInputInfo[key] = KEY_RELEASE;
            }
            else if (m_pInputInfo[key] == KEY_RELEASE)
            {
                m_pInputInfo[key] = KEY_NONE;
            }
        }
    }

    if (IsPress(VK_RETURN) && IsEnterPressed() == false)
    {
        m_pInputInfo[VK_RETURN] = KEY_NONE;
    }
    SetEnterPressed(false);
}

bool SEASON3B::CNewKeyInput::IsNone(int iVirtKey) const
{
#ifdef ASG_FIX_ACTIVATE_APP_INPUT
    if (SDL_GetKeyboardFocus() == nullptr)
        return false;
#endif // ASG_FIX_ACTIVATE_APP_INPUT
    return (m_pInputInfo[iVirtKey] == KEY_NONE) ? true : false;
}

bool SEASON3B::CNewKeyInput::IsRelease(int iVirtKey) const
{
#ifdef ASG_FIX_ACTIVATE_APP_INPUT
    if (SDL_GetKeyboardFocus() == nullptr)
        return false;
#endif // ASG_FIX_ACTIVATE_APP_INPUT
    return (m_pInputInfo[iVirtKey] == KEY_RELEASE) ? true : false;
}

bool SEASON3B::CNewKeyInput::IsPress(int iVirtKey) const
{
#ifdef ASG_FIX_ACTIVATE_APP_INPUT
    if (SDL_GetKeyboardFocus() == nullptr)
        return false;
#endif // ASG_FIX_ACTIVATE_APP_INPUT
    return (m_pInputInfo[iVirtKey] == KEY_PRESS) ? true : false;
}

bool SEASON3B::CNewKeyInput::IsRepeat(int iVirtKey) const
{
#ifdef ASG_FIX_ACTIVATE_APP_INPUT
    if (SDL_GetKeyboardFocus() == nullptr)
        return false;
#endif // ASG_FIX_ACTIVATE_APP_INPUT
    return (m_pInputInfo[iVirtKey] == KEY_REPEAT) ? true : false;
}

void SEASON3B::CNewKeyInput::SetKeyState(int iVirtKey, KEY_STATE KeyState)
{
    m_pInputInfo[iVirtKey] = KeyState;
}

const BYTE *SEASON3B::CNewKeyInput::StateData() const noexcept
{
    return m_pInputInfo;
}

bool SEASON3B::CNewKeyInput::IsEnterPressed()
{
    return g_bEnterPressed;
}

void SEASON3B::CNewKeyInput::SetEnterPressed(bool enterPressed)
{
    g_bEnterPressed = enterPressed;
}

bool ApplicationLegacyCalls::IsNone(int virtualKey) const
{
    return applicationKeeper_.NewKeyInputUnit()->IsNone(virtualKey);
}

bool ApplicationLegacyCalls::IsRelease(int virtualKey) const
{
    return applicationKeeper_.NewKeyInputUnit()->IsRelease(virtualKey);
}

bool ApplicationLegacyCalls::IsPress(int virtualKey) const
{
    return applicationKeeper_.NewKeyInputUnit()->IsPress(virtualKey);
}

bool ApplicationLegacyCalls::IsRepeat(int virtualKey) const
{
    return applicationKeeper_.NewKeyInputUnit()->IsRepeat(virtualKey);
}

void ApplicationSupportCalls::SetKeyState(int virtualKey, int state)
{
    applicationKeeper_.NewKeyInputUnit()->SetKeyState(
        virtualKey, static_cast<SEASON3B::CNewKeyInput::KEY_STATE>(state));
}

bool SessionLegacyCalls::IsKeyDown(int virtualKey) const
{
    return sessionKeeper_.InputKeyIsDown(virtualKey);
}

bool SessionLegacyCalls::IsNone(int virtualKey) const
{
    return sessionKeeper_.InputKeyIsNone(virtualKey);
}

bool SessionLegacyCalls::IsRelease(int virtualKey) const
{
    return sessionKeeper_.InputKeyIsRelease(virtualKey);
}

bool SessionLegacyCalls::IsPress(int virtualKey) const
{
    return sessionKeeper_.InputKeyIsPress(virtualKey);
}

bool SessionLegacyCalls::IsRepeat(int virtualKey) const
{
    return sessionKeeper_.InputKeyIsRepeat(virtualKey);
}

void SessionLegacyCalls::SetKeyState(int virtualKey, int state)
{
    sessionKeeper_.SetInputKeyState(virtualKey, state);
}

CInput &ApplicationSupportCalls::Input()
{
    return *applicationKeeper_.InputUnit();
}

bool ApplicationLegacyCalls::IsEnterPressed()
{
    return applicationKeeper_.NewKeyInputUnit()->IsEnterPressed();
}

void ApplicationLegacyCalls::SetEnterPressed(bool enterPressed)
{
    applicationKeeper_.NewKeyInputUnit()->SetEnterPressed(enterPressed);
}

//	NewUIGroup.cpp

using namespace SEASON3B;

CNewUIGroup::CNewUIGroup(SessionKeeper &keeper) : SessionUiLegacyBindings(keeper)
{
    m_vecUI.clear();
}

CNewUIGroup::~CNewUIGroup()
{
    Release();
}

void CNewUIGroup::AddUIObj(CNewUIObj *pUIObj)
{
    m_vecUI.push_back(pUIObj);
}

bool CNewUIGroup::Update()
{
    if (IsEnabled() == false)
        return false;

    auto vi = m_vecUI.begin();
    for (; vi != m_vecUI.end(); vi++)
    {
        if ((*vi)->IsEnabled() == true)
        {
            if ((*vi)->Update() == false)
            {
                return false;
            }
        }
    }

    return true;
}

bool CNewUIGroup::UpdateMouseEvent()
{
    auto vi = m_vecUI.begin();

    for (; vi != m_vecUI.end(); vi++)
    {
        if ((*vi)->IsVisible())
        {
            CNewUIObj *pUIObj = (*vi);
            pUIObj->UpdateMouseEvent();
            // 			if( pUIObj->UpdateMouseEvent() == true )
            // 				break;
        }
    }

    return true;
}

bool CNewUIGroup::UpdateKeyEvent()
{
    auto vi = m_vecUI.begin();
    for (; vi != m_vecUI.end(); vi++)
    {
        HWND hRelatedWnd = (*vi)->GetRelatedWnd();
        if (NULL == hRelatedWnd)
        {
            hRelatedWnd = g_hWnd;
        }

        HWND hWnd = GetFocus();

        if ((*vi)->IsEnabled() && hWnd == hRelatedWnd)
        {
            CNewUIObj *pUIObj = (*vi);
            pUIObj->UpdateKeyEvent();
            // 			if( pUIObj->UpdateKeyEvent() == true )
            // 				break;
        }
    }

    return true;
}

void CNewUIGroup::Release()
{
    auto vi = m_vecUI.begin();
    for (; vi != m_vecUI.end(); vi++)
    {
        CNewUIObj *pUIObj = (*vi);
        SAFE_DELETE(pUIObj);
    }

    int iCount = 0;

    vi = m_vecUI.begin();
    for (; vi < m_vecUI.end(); ++vi)
    {
        CNewUIObj *pUIObj = (*vi);
        if (pUIObj != NULL)
        {
            __TraceF(TEXT("vecUI \n"), iCount);
        }
        iCount++;
    }

    m_vecUI.clear();
}

CUIFriendMenu *CreateSessionFriendMenu(SessionKeeper &keeper)
{
    return new CUIFriendMenu(keeper);
}

void DestroySessionFriendMenu(CUIFriendMenu *friendMenu) noexcept
{
    delete friendMenu;
}

CUIWindowMgr::CUIWindowMgr(SessionKeeper &keeper)
    : SessionUiLegacyBindings(keeper), gameplay_(GameplayForConstruction())
{
    m_bWindowsEnable = FALSE;
    m_WorkMessage = {};
    m_bRenderFrame = TRUE;
    m_dwMainWindowUIID = 0;

    m_iMainWindowPos_x = 0;
    m_iMainWindowPos_y = 0;
    m_iMainWindowWidth = 0;
    m_iMainWindowHeight = 0;
    m_iMainWindowBackPos_y = 0;
    m_iMainWindowBackHeight = 0;
    m_bIsMainWindowMaximize = FALSE;
    m_bChatReject = FALSE;
    m_iLastFriendWindowTabIndex = 0;

    frameTimerScheduler_.SetRepeating(CHATCONNECT_TIMER, 15 * 1000, [this] {
        SessionOrigin().FriendMenuObject().SendChatRoomConnectCheck();
    });

    g_iLetterReadNextPos_x = UIWND_DEFAULT;
    g_iLetterReadNextPos_y = UIWND_DEFAULT;
}

CUIWindowMgr::~CUIWindowMgr()
{
    frameTimerScheduler_.Kill(CHATCONNECT_TIMER);
    Reset();
}

void CUIWindowMgr::Reset()
{
    m_dwMainWindowUIID = 0;
    for (m_WindowMapIter = m_WindowMap.begin(); m_WindowMapIter != m_WindowMap.end();
         ++m_WindowMapIter)
    {
        if (m_WindowMapIter->second != NULL)
        {
            delete m_WindowMapIter->second;
            m_WindowMapIter->second = NULL;
        }
    }
    m_WindowMap.clear();
    m_WindowArrangeList.clear();
    m_LetterReadMap.clear();

    HideAllWindowClear();
    m_ForceTopWindowList.clear();
    g_dwTopWindow = 0;
    m_iLastFriendWindowTabIndex = 0;
    m_bServerEnable = TRUE;
    m_iFriendMainWindowTitleNumber = 990;
    m_dwAddWindowUIID = 0;
    SetChatReject(FALSE);
    if (GetFriendMainWindow() != NULL)
    {
        GetFriendMainWindow()->Reset();
    }
    if (g_iChatInputType == 0)
    {
        // for OpenMU the following line is not required.
        // 2 probably means logging out.
        SocketClient->ToGameServer()->SendSetFriendOnlineState(2);
    }
}

DWORD CUIWindowMgr::AddWindow(int iWindowType, int iPos_x, int iPos_y, const wchar_t *pszTitle,
                              DWORD dwParentID, int iOption)
{
    if (g_iChatInputType == 0 /* || g_dwTopWindow != 0*/)
        return 0;
    CUIBaseWindow *pbw = NULL;

    switch (iWindowType)
    {
    case UIWNDTYPE_EMPTY:
        pbw = new CUIDefaultWindow(SessionOrigin());
        break;
    case UIWNDTYPE_CHAT:
    case UIWNDTYPE_CHAT_READY:
        pbw = new CUIChatWindow(SessionOrigin());
        if (m_dwMainWindowUIID != 0)
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), pszTitle);
        }
        break;
    case UIWNDTYPE_FRIENDMAIN:
        if (m_dwMainWindowUIID == 0)
        {
            pbw = new CUIFriendWindow(SessionOrigin());
            m_dwMainWindowUIID = pbw->GetUIID();
            if (SessionOrigin().FriendMenuObject().IsNewMailAlert() == TRUE)
            {
                ((CUIFriendWindow *)pbw)->SetTabIndex(1);
            }
            else
            {
                ((CUIFriendWindow *)pbw)->SetTabIndex(m_iLastFriendWindowTabIndex);
            }
            SessionOrigin().FriendMenuObject().SetNewMailAlert(FALSE);
            if (IsServerEnable() == FALSE)
            {
                SocketClient->ToGameServer()->SendFriendListRequest();
            }
        }
        else
            return 0;
        break;
    case UIWNDTYPE_TEXTINPUT:
        if (g_dwTopWindow != 0)
            return 0;
        pbw = new CUITextInputWindow(SessionOrigin());
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), pszTitle);
        }
        g_dwTopWindow = pbw->GetUIID();
        break;
    case UIWNDTYPE_QUESTION:
    case UIWNDTYPE_QUESTION_FORCE:
        pbw = new CUIQuestionWindow(SessionOrigin(), 0);
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);

            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), I18N::Game::Question);
        }
        if (iWindowType == UIWNDTYPE_QUESTION)
            g_dwTopWindow = pbw->GetUIID();
        else
            AddForceTopWindowList(pbw->GetUIID());
        break;
    case UIWNDTYPE_OK:
        if (g_dwTopWindow != 0)
            return 0;
    case UIWNDTYPE_OK_FORCE:
        pbw = new CUIQuestionWindow(SessionOrigin(), 1);
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), I18N::Game::OK);
        }
        if (iWindowType == UIWNDTYPE_OK)
            g_dwTopWindow = pbw->GetUIID();
        else
            AddForceTopWindowList(pbw->GetUIID());
        break;
    case UIWNDTYPE_READLETTER:
        pbw = new CUILetterReadWindow(SessionOrigin());
        if (m_dwMainWindowUIID != 0)
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), pszTitle);
        }
        break;
    case UIWNDTYPE_WRITELETTER:
        if (g_dwTopWindow != 0)
            return 0;
        pbw = new CUILetterWriteWindow(SessionOrigin());
        if (m_dwMainWindowUIID != 0)
        {
            auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
            if (pMainWnd != NULL)
                pMainWnd->AddWindow(pbw->GetUIID(), pszTitle);
        }
        break;
    default:
        return 0;
        break;
    };

    if (iPos_x == UIWND_DEFAULT)
        iPos_x = 0;
    if (iPos_y == UIWND_DEFAULT)
        iPos_y = 332;
    if (!pbw)
        return 0;

    pbw->Init(pszTitle, dwParentID);

    if (!(iOption & UIADDWND_FORCEPOSITION))
    {
        for (m_WindowMapIter = m_WindowMap.begin(); m_WindowMapIter != m_WindowMap.end();
             ++m_WindowMapIter)
        {
            if (m_WindowMapIter->second->GetPosition_x() == iPos_x &&
                m_WindowMapIter->second->GetPosition_y() == iPos_y)
            {
                if (iPos_x + pbw->GetWidth() + 20 <= REFERENCE_WIDTH)
                    iPos_x += 20;
                if (iPos_y + pbw->GetHeight() + 20 <= REFERENCE_HEIGHT)
                    iPos_y += 20;
                if (iPos_x + pbw->GetWidth() + 20 > REFERENCE_WIDTH &&
                    iPos_y + pbw->GetHeight() + 20 > REFERENCE_HEIGHT)
                {
                    if (iPos_y % 10 == 9)
                    {
                        delete pbw;
                        return 0;
                    }
                    iPos_x = iPos_y = iPos_y % 10 + 1;
                }
                m_WindowMapIter = m_WindowMap.begin();
            }
        }
    }
    pbw->SetPosition(iPos_x, iPos_y);

    DWORD dwUIID = pbw->GetUIID();

    m_WindowMap.insert(std::pair<DWORD, CUIBaseWindow *>(dwUIID, pbw));
    m_WindowArrangeList.push_back(dwUIID);
    if (iWindowType == UIWNDTYPE_CHAT || iWindowType == UIWNDTYPE_CHAT_READY)
        SessionOrigin().FriendMenuObject().AddWindow(dwUIID, pbw);

    pbw->Refresh();
    if (iWindowType == UIWNDTYPE_CHAT)
    {
        ((CUIChatWindow *)pbw)->FocusReset();
    }

    return dwUIID;
}

void CUIWindowMgr::RemoveWindow(DWORD dwUIID)
{
    if (m_dwMainWindowUIID == dwUIID)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_FRIEND);

        if (g_dwTopWindow != 0)
        {
            return;
        }

        CUIBaseWindow *pWindow = GetWindow(m_dwMainWindowUIID);
        if (pWindow != NULL)
        {
            m_iMainWindowPos_x = pWindow->GetPosition_x();
            m_iMainWindowPos_y = pWindow->GetPosition_y();
            m_iMainWindowWidth = pWindow->GetWidth();
            m_iMainWindowHeight = pWindow->GetHeight();
            pWindow->GetBackPosition(&m_bIsMainWindowMaximize, &m_iMainWindowBackPos_y,
                                     &m_iMainWindowBackHeight);
            m_iLastFriendWindowTabIndex = ((CUIFriendWindow *)pWindow)->GetTabIndex();
        }
        m_dwMainWindowUIID = 0;
    }

    if (GetAddFriendWindow() == dwUIID)
    {
        SetAddFriendWindow(0);
    }

    m_WindowMapIter = m_WindowMap.find(dwUIID);
    if (m_WindowMapIter == m_WindowMap.end())
    {
        return;
    }

    RemoveForceTopWindowList(dwUIID);

    if (m_WindowMapIter->second != NULL)
    {
        delete m_WindowMapIter->second;
        m_WindowMapIter->second = NULL;
    }
    m_WindowMap.erase(m_WindowMapIter);
    m_WindowArrangeList.remove(dwUIID);
    if (m_WindowArrangeList.empty())
        SetWindowsEnable(FALSE);

    if (m_dwMainWindowUIID != 0)
    {
        auto *pMainWnd = (CUIFriendWindow *)GetWindow(m_dwMainWindowUIID);
        if (pMainWnd != NULL)
            pMainWnd->RemoveWindow(dwUIID);
    }
    SessionOrigin().FriendMenuObject().RemoveWindow(dwUIID);

    if (g_dwTopWindow == dwUIID)
    {
        g_dwTopWindow = 0;
    }
}

void RenderColor(float x, float y, float Width, float Height, float Alpha, int Flag);

bool CUIWindowMgr::ProcessModernUiInput(const SessionInputEvent &event)
{
    for (auto it = m_WindowArrangeList.rbegin(); it != m_WindowArrangeList.rend(); ++it)
    {
        const auto window = m_WindowMap.find(*it);
        if (window == m_WindowMap.end() || window->second == nullptr ||
            window->second->GetState() == UISTATE_HIDE ||
            window->second->GetState() == UISTATE_READY)
        {
            continue;
        }
        if (window->second->ProcessModernUiInput(event))
            return true;
    }
    return false;
}

std::optional<UI::Modern::RmlTextInputArea> CUIWindowMgr::ModernTextInputArea() const
{
    for (auto it = m_WindowArrangeList.rbegin(); it != m_WindowArrangeList.rend(); ++it)
    {
        const auto window = m_WindowMap.find(*it);
        if (window == m_WindowMap.end() || window->second == nullptr ||
            window->second->GetState() == UISTATE_HIDE ||
            window->second->GetState() == UISTATE_READY)
        {
            continue;
        }
        if (auto area = window->second->ModernTextInputArea())
            return area;
    }
    return std::nullopt;
}

void CUIWindowMgr::DoAction()
{
    if (g_dwTopWindow != 0)
    {
        if (g_pWindowMgr->GetWindow(g_dwTopWindow) == NULL)
        {
            g_dwTopWindow = 0;
        }
    }

    if (GetFocus() == g_hWnd && PressKey(VK_F5))
    {
        SessionOrigin().FriendMenuObject().ShowMenu(TRUE);
    }

    if (PressKey(VK_F6))
    {
        if ((m_bCurrentHideWindowState == FALSE || m_HideWindowList.empty() == FALSE))
        {
            g_pWindowMgr->HideAllWindow(TRUE, TRUE);
        }
    }

    if (m_dwMainWindowUIID > 0 && GetFriendMainWindow()->GetState() == UISTATE_HIDE)
    {
        CloseMainWnd();
    }

    for (m_WindowReverseArrangeListIter = m_WindowArrangeList.rbegin();
         m_WindowReverseArrangeListIter != m_WindowArrangeList.rend();
         ++m_WindowReverseArrangeListIter)
    {
        m_WindowMapIter = m_WindowMap.find(*m_WindowReverseArrangeListIter);
        if (m_WindowMapIter != m_WindowMap.end())
        {
            if (m_WindowMapIter->second->GetState() != UISTATE_HIDE &&
                m_WindowMapIter->second->GetState() != UISTATE_READY)
                m_WindowMapIter->second->DoAction();
        }
    }

    while (m_MessageList.empty() == FALSE)
    {
        GetUIMessage();
        HandleMessage();
    }

    m_bRenderFrame = FALSE;
}

void CUIWindowMgr::ShowHideWindow(DWORD dwUIID, BOOL bShowWindow)
{
    CUIBaseWindow *pWindow = GetWindow(dwUIID);
    if (pWindow != NULL)
    {
        if (bShowWindow == TRUE)
        {
            pWindow->SetState(UISTATE_NORMAL);
        }
        else
            pWindow->SetState(UISTATE_HIDE);
    }
}

void CUIWindowMgr::HideAllWindow(BOOL bHide, BOOL bMainClose)
{
    int iCount = 0;
    for (m_WindowMapIter = m_WindowMap.begin(); m_WindowMapIter != m_WindowMap.end();
         ++m_WindowMapIter)
    {
        if ((bMainClose == TRUE || m_WindowMapIter->first != m_dwMainWindowUIID) &&
            m_WindowMapIter->first != g_dwTopWindow)
        {
            if (bHide == TRUE)
            {
                if (m_WindowMapIter->second->GetState() == UISTATE_NORMAL)
                {
                    if (bMainClose == TRUE)
                        m_HideWindowList.push_back(m_WindowMapIter->first);
                    m_WindowMapIter->second->SetState(UISTATE_HIDE);
                }
            }
            else
            {
                for (std::list<DWORD>::iterator iter = m_HideWindowList.begin();
                     iter != m_HideWindowList.end(); ++iter)
                {
                    if (m_WindowMapIter->first == *iter)
                    {
                        ++iCount;
                        m_WindowMapIter->second->SetState(UISTATE_NORMAL);
                    }
                }
            }
        }
    }
    if (bHide == FALSE)
    {
        int iHideSize = m_HideWindowList.size();
        if (iHideSize - iCount > 0)
        {
            OpenMainWnd(REFERENCE_WIDTH - 250, 432 - 170);
        }
        if (iCount > 0 && GetTopNotMainWindowUIID() > 0)
        {
            SendUIMessage(UI_MESSAGE_SELECT, GetTopNotMainWindowUIID(), 0);
        }
        m_HideWindowList.clear();
    }
    else
    {
        SetWindowsEnable(FALSE);
        SetFocus(g_hWnd);
        ReleaseTextInputFocus();
    }
}

void CUIWindowMgr::HideAllWindowClear()
{
    m_bCurrentHideWindowState = FALSE;
    m_HideWindowList.clear();
}

DWORD CUIWindowMgr::GetTopNotMainWindowUIID()
{
    if (m_WindowArrangeList.empty() == TRUE)
        return 0;
    m_WindowReverseArrangeListIter = m_WindowArrangeList.rbegin();
    DWORD dwResult = *m_WindowReverseArrangeListIter;
    if (dwResult == m_dwMainWindowUIID)
    {
        if (m_WindowArrangeList.size() == 1)
            return 0;
        else
            dwResult = *(++m_WindowReverseArrangeListIter);
    }
    return dwResult;
}

void CUIWindowMgr::AddWindowFinder(CUIBaseWindow *pWindow)
{
    if (pWindow == NULL)
        return;
    DWORD dwUIID = pWindow->GetUIID();
    m_WindowFindMap.insert(std::pair<DWORD, CUIBaseWindow *>(dwUIID, pWindow));
}

void CUIWindowMgr::RemoveWindowFinder(DWORD dwUIID)
{
    m_WindowMapIter = m_WindowFindMap.find(dwUIID);
    m_WindowFindMap.erase(m_WindowMapIter);
}

CUIBaseWindow *CUIWindowMgr::GetWindow(DWORD dwUIID)
{
    m_WindowMapIter = m_WindowMap.find(dwUIID);
    if (m_WindowMapIter == m_WindowMap.end())
    {
        m_WindowMapIter = m_WindowFindMap.find(dwUIID);
        if (m_WindowMapIter == m_WindowFindMap.end())
        {
            return NULL;
        }
        else
        {
            return m_WindowMapIter->second;
        }
    }
    else
        return m_WindowMapIter->second;
}

BOOL CUIWindowMgr::IsWindow(DWORD dwUIID)
{
    m_WindowMapIter = m_WindowMap.find(dwUIID);
    if (m_WindowMapIter == m_WindowMap.end())
    {
        m_WindowMapIter = m_WindowFindMap.find(dwUIID);
        if (m_WindowMapIter == m_WindowFindMap.end())
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    else
        return TRUE;
}

void CUIWindowMgr::SendUIMessageToWindow(DWORD dwUIID, int iMessage, LONG_PTR iParam1,
                                         LONG_PTR iParam2, std::wstring text)
{
    CUIBaseWindow *pWindow = GetWindow(dwUIID);
    if (pWindow != NULL)
    {
        pWindow->SendUIMessage(iMessage, iParam1, iParam2, std::move(text));
    }
}

void CUIWindowMgr::HandleMessage()
{
    assert(m_WorkMessage.m_iParam1 != 0 && "Error Handle message");

    switch (m_WorkMessage.m_iMessage)
    {
    case UI_MESSAGE_SELECT:
        if (m_WorkMessage.m_iParam1 != 0 && GetWindow(m_WorkMessage.m_iParam1) != NULL)
        {
            if (GetWindow(m_WorkMessage.m_iParam1)->HaveTextBox() == FALSE)
            {
                if ((GetWindow(GetTopWindowUIID()) != NULL &&
                     GetWindow(GetTopWindowUIID())->HaveTextBox() == TRUE) ||
                    g_pSingleTextInputBox->HaveFocus() == TRUE)
                    SaveIMEStatus();

                SetFocus(g_hWnd);
                ReleaseTextInputFocus();
            }
            if (GetWindow(m_WorkMessage.m_iParam1)->GetState() == UISTATE_HIDE)
                ShowHideWindow(m_WorkMessage.m_iParam1, TRUE);

            m_WindowArrangeListIter = m_WindowArrangeList.end();
            --m_WindowArrangeListIter;

            if ((int)(*m_WindowArrangeListIter) != m_WorkMessage.m_iParam1 || GetFocus() == g_hWnd)
            {
                m_WindowArrangeList.remove(m_WorkMessage.m_iParam1);
                m_WindowArrangeList.push_back(m_WorkMessage.m_iParam1);
                SendUIMessageToWindow(m_WorkMessage.m_iParam1, UI_MESSAGE_SELECTED, 0, 0);
            }

            SetWindowsEnable(m_WorkMessage.m_iParam1);
            SessionOrigin().FriendMenuObject().SetNewChatAlertOff(m_WorkMessage.m_iParam1);
            SessionOrigin().FriendMenuObject().HideMenu();
        }
        break;
    case UI_MESSAGE_HIDE:
        if (m_WorkMessage.m_iParam1 != 0)
        {
            if (g_dwTopWindow != 0 && m_dwMainWindowUIID != 0 &&
                m_WorkMessage.m_iParam1 == (int)m_dwMainWindowUIID)
                break;

            PlayBuffer(SOUND_CLICK01);
            ShowHideWindow(m_WorkMessage.m_iParam1, FALSE);
            m_WindowArrangeList.remove(m_WorkMessage.m_iParam1);
            m_WindowArrangeList.push_front(m_WorkMessage.m_iParam1);
            if (GetTopWindowUIID() != 0)
            {
                CUIBaseWindow *pWindow = GetWindow(GetTopWindowUIID());
                if (pWindow != NULL)
                {
                    if (pWindow->GetState() != UISTATE_HIDE && pWindow->GetState() != UISTATE_READY)
                    {
                        //GetWindow(GetTopWindowUIID())->SetState(UISTATE_NORMAL);
                        SendUIMessageToWindow(GetTopWindowUIID(), UI_MESSAGE_SELECTED, 0, 0);
                    }
                    else
                    {
                        g_pWindowMgr->SetWindowsEnable(FALSE);
                        SetFocus(g_hWnd);
                        ReleaseTextInputFocus();
                    }
                }
            }
            else
            {
                g_pWindowMgr->SetWindowsEnable(FALSE);
                SetFocus(g_hWnd);
                ReleaseTextInputFocus();
            }
        }
        break;
    case UI_MESSAGE_MAXIMIZE:
        if (m_WorkMessage.m_iParam1 != 0)
        {
            if (GetWindow(m_WorkMessage.m_iParam1) != NULL)
            {
                GetWindow(m_WorkMessage.m_iParam1)->Maximize();
                PlayBuffer(SOUND_CLICK01);
            }
        }
        break;
    case UI_MESSAGE_CLOSE:
        if (m_WorkMessage.m_iParam1 != 0)
        {
            PlayBuffer(SOUND_CLICK01);

            if (GetWindow(m_WorkMessage.m_iParam1) != NULL &&
                GetWindow(m_WorkMessage.m_iParam1)->HaveTextBox() == TRUE)
            {
                SaveIMEStatus();
            }
            RemoveWindow(m_WorkMessage.m_iParam1);
            if (GetTopWindowUIID() != 0)
            {
                CUIBaseWindow *pWindow = GetWindow(GetTopWindowUIID());
                if (pWindow != NULL)
                {
                    if (pWindow->GetState() != UISTATE_HIDE && pWindow->GetState() != UISTATE_READY)
                    {
                        //GetWindow(GetTopWindowUIID())->SetState(UISTATE_NORMAL);
                        SendUIMessageToWindow(GetTopWindowUIID(), UI_MESSAGE_SELECTED, 0, 0);
                    }
                    else
                    {
                        g_pWindowMgr->SetWindowsEnable(FALSE);
                        SetFocus(g_hWnd);
                        ReleaseTextInputFocus();
                    }
                }
            }
            else
            {
                g_pWindowMgr->SetWindowsEnable(FALSE);
                SetFocus(g_hWnd);
                ReleaseTextInputFocus();
            }
        }
        break;
    case UI_MESSAGE_BOTTOM:
        if (m_WorkMessage.m_iParam1 != 0 && GetWindow(m_WorkMessage.m_iParam1) != NULL)
        {
            if ((int)(*m_WindowArrangeList.begin()) != m_WorkMessage.m_iParam1)
            {
                m_WindowArrangeList.remove(m_WorkMessage.m_iParam1);
                m_WindowArrangeList.push_front(m_WorkMessage.m_iParam1);
            }
        }
        break;
    default:
        break;
    }
}

void CUIWindowMgr::OpenMainWnd(int iPos_x, int iPos_y)
{
    g_pWindowMgr->HideAllWindowClear();
    if (g_iChatInputType == 0)
    {
        if (g_pSystemLogBox->CheckChatRedundancy(I18N::Game::YouCannotUseTheMyFriend, 2) == FALSE)
            g_pSystemLogBox->AddText(I18N::Game::YouCannotUseTheMyFriend,
                                     SEASON3B::TYPE_SYSTEM_MESSAGE);
        return;
    }
    int iLevel = CharacterAttribute->Level;

    if (iLevel < 6)
    {
        if (g_pSystemLogBox->CheckChatRedundancy(
                I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction) == FALSE)
            g_pSystemLogBox->AddText(I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction,
                                     SEASON3B::TYPE_SYSTEM_MESSAGE);
        return;
    }

    if (m_dwMainWindowUIID != 0)
    {
        if (GetWindow(m_dwMainWindowUIID)->GetState() == UISTATE_HIDE)
        {
            ShowHideWindow(m_dwMainWindowUIID, TRUE);
            PlayBuffer(SOUND_CLICK01);
            PlayBuffer(SOUND_INTERFACE01);
        }
        else if (GetWindow(m_dwMainWindowUIID)->GetState() == UISTATE_NORMAL)
        {
            CloseMainWnd();
        }
        return;
    }
    if (m_iMainWindowWidth == 0)
    {
        AddWindow(UIWNDTYPE_FRIENDMAIN, iPos_x, iPos_y,
                  I18N::Game::Lookup(m_iFriendMainWindowTitleNumber));
        g_pWindowMgr->SendUIMessage(UI_MESSAGE_SELECT, m_dwMainWindowUIID, 0);
        RefreshMainWndChatRoomList();
        PlayBuffer(SOUND_CLICK01);
        PlayBuffer(SOUND_INTERFACE01);
    }
    else
    {
        AddWindow(UIWNDTYPE_FRIENDMAIN, m_iMainWindowPos_x, m_iMainWindowPos_y,
                  I18N::Game::Lookup(m_iFriendMainWindowTitleNumber));
        g_pWindowMgr->SendUIMessage(UI_MESSAGE_SELECT, m_dwMainWindowUIID, 0);
        CUIBaseWindow *pWindow = GetWindow(m_dwMainWindowUIID);
        if (pWindow != NULL)
        {
            pWindow->SetSize(m_iMainWindowWidth, m_iMainWindowHeight);
            pWindow->SetBackPosition(m_bIsMainWindowMaximize, m_iMainWindowBackPos_y,
                                     m_iMainWindowBackHeight);
            // 윈도우 목록 복구
            RefreshMainWndChatRoomList();
            pWindow->Refresh();
            //			((CUIFriendWindow *)pWindow)->SetTabIndex(m_iLastFriendWindowTabIndex);
            PlayBuffer(SOUND_CLICK01);
            PlayBuffer(SOUND_INTERFACE01);
        }
        else
        {
            return;
        }
        DoAction();
    }
}

void CUIWindowMgr::CloseMainWnd()
{
    if (m_dwMainWindowUIID == 0)
        return;
    RemoveWindow(m_dwMainWindowUIID);
    //SendUIMessage(UI_MESSAGE_CLOSE, m_dwMainWindowUIID, 0);
}

void CUIWindowMgr::RefreshMainWndChatRoomList()
{
    CUIBaseWindow *pWindow = GetWindow(m_dwMainWindowUIID);
    if (pWindow == NULL)
        return;
    ((CUIFriendWindow *)pWindow)->ResetWindow();
    for (m_WindowMapIter = m_WindowMap.begin(); m_WindowMapIter != m_WindowMap.end();
         ++m_WindowMapIter)
    {
        if (m_dwMainWindowUIID != m_WindowMapIter->first &&
            m_WindowMapIter->second->GetState() != UISTATE_READY)
            ((CUIFriendWindow *)pWindow)
                ->AddWindow(m_WindowMapIter->first, m_WindowMapIter->second->GetTitle());
    }
}

BOOL CUIWindowMgr::LetterReadCheck(DWORD dwLetterID)
{
    m_LetterReadMapIter = m_LetterReadMap.find(dwLetterID);
    if (m_LetterReadMapIter == m_LetterReadMap.end())
    {
        m_LetterReadMap.insert(std::pair<DWORD, DWORD>(dwLetterID, 0));
        return FALSE;
    }
    return TRUE;
}

void CUIWindowMgr::CloseLetterRead(DWORD dwLetterID)
{
    m_LetterReadMapIter = m_LetterReadMap.find(dwLetterID);
    if (m_LetterReadMapIter != m_LetterReadMap.end())
    {
        m_LetterReadMap.erase(m_LetterReadMapIter);
    }
}

void CUIWindowMgr::SetLetterReadWindow(DWORD dwLetterID, DWORD dwWindowUIID)
{
    m_LetterReadMapIter = m_LetterReadMap.find(dwLetterID);
    if (m_LetterReadMapIter != m_LetterReadMap.end())
    {
        m_LetterReadMapIter->second = dwWindowUIID;
    }
}

DWORD CUIWindowMgr::GetLetterReadWindow(DWORD dwLetterID)
{
    m_LetterReadMapIter = m_LetterReadMap.find(dwLetterID);
    if (m_LetterReadMapIter != m_LetterReadMap.end())
    {
        return m_LetterReadMapIter->second;
    }
    return 0;
}

void CUIWindowMgr::AddForceTopWindowList(DWORD dwWindowUIID)
{
    if (IsForceTopWindow(dwWindowUIID) == FALSE)
        m_ForceTopWindowList.push_back(dwWindowUIID);
}

void CUIWindowMgr::RemoveForceTopWindowList(DWORD dwWindowUIID)
{
    m_ForceTopWindowList.remove(dwWindowUIID);
}

BOOL CUIWindowMgr::IsForceTopWindow(DWORD dwWindowUIID)
{
    for (std::list<DWORD>::iterator iter = m_ForceTopWindowList.begin();
         iter != m_ForceTopWindowList.end(); ++iter)
    {
        if (*iter == dwWindowUIID)
            return TRUE;
    }
    return FALSE;
}

void CUIWindowMgr::SetServerEnable(BOOL bFlag)
{
    m_bServerEnable = bFlag;
    if (bFlag == TRUE)
    {
        if (m_iFriendMainWindowTitleNumber != 990)
        {
            m_iFriendMainWindowTitleNumber = 990;
            if (GetFriendMainWindow() != NULL)
                GetFriendMainWindow()->SetTitle(I18N::Game::Lookup(m_iFriendMainWindowTitleNumber));
        }
    }
    else
    {
        if (m_iFriendMainWindowTitleNumber != 1066)
        {
            m_iFriendMainWindowTitleNumber = 1066;
            if (GetFriendMainWindow() != NULL)
                GetFriendMainWindow()->SetTitle(I18N::Game::Lookup(m_iFriendMainWindowTitleNumber));
        }
    }
}

extern void MoveCharacter(CHARACTER *c, OBJECT *o);

void SessionUiUnit::ReceiveChatRoomConnectResult(DWORD dwWindowUIID, const BYTE *ReceiveBuffer)
{
    auto Data = (LPFS_CHAT_JOIN_RESULT)ReceiveBuffer;
    switch (Data->Result)
    {
    case 0x00:
        g_pWindowMgr->AddWindow(UIWNDTYPE_OK_FORCE, UIWND_DEFAULT, UIWND_DEFAULT,
                                I18N::Game::ChatRoomIsFull);
        break;
    case 0x01:
        break;
    default:
        break;
    };
}

void SessionUiUnit::ReceiveChatRoomUserStateChange(DWORD dwWindowUIID, const BYTE *ReceiveBuffer)
{
    auto Data = (LPFS_CHAT_CHANGE_STATE)ReceiveBuffer;
    auto *pChatWindow = (CUIChatWindow *)g_pWindowMgr->GetWindow(dwWindowUIID);
    if (pChatWindow == NULL)
        return;
    wchar_t szName[MAX_USERNAME_SIZE + 1] = {0};
    CMultiLanguage::ConvertFromUtf8(szName, Data->Name, MAX_USERNAME_SIZE);
    szName[MAX_USERNAME_SIZE] = '\0';
    wchar_t szText[MAX_TEXT_LENGTH + 1] = {0};
    CMultiLanguage::ConvertFromUtf8(szText, Data->Name, MAX_USERNAME_SIZE);
    szText[MAX_USERNAME_SIZE] = '\0';
    switch (Data->Type)
    {
    case 0x00:
        if (pChatWindow->AddChatPal(szName, Data->Index, 0) >= 3)
        {
            wcscat(szText, I18N::Game::HasEntered);
            pChatWindow->AddChatText(255, szText, 1, 0);
        }
        break;
    case 0x01:
        if (pChatWindow->GetUserCount() >= 3)
        {
            wcscat(szText, I18N::Game::HasLeft);
            pChatWindow->AddChatText(255, szText, 1, 0);
        }
        pChatWindow->RemoveChatPal(szName);
        break;
    default:
        return;
        break;
    };
    if (pChatWindow->GetShowType() == 2)
        pChatWindow->UpdateInvitePalList();
}

void SessionUiUnit::ReceiveChatRoomUserList(DWORD dwWindowUIID, const BYTE *ReceiveBuffer)
{
    auto Header = (LPFS_CHAT_USERLIST_HEADER)ReceiveBuffer;
    int iMoveOffset = sizeof(FS_CHAT_USERLIST_HEADER);
    wchar_t szName[MAX_USERNAME_SIZE + 1] = {0};
    for (int i = 0; i < Header->Count; ++i)
    {
        auto Data = (LPFS_CHAT_USERLIST_DATA)(ReceiveBuffer + iMoveOffset);
        CMultiLanguage::ConvertFromUtf8(szName, Data->Name, MAX_USERNAME_SIZE);
        szName[MAX_USERNAME_SIZE] = '\0';
        ((CUIChatWindow *)g_pWindowMgr->GetWindow(dwWindowUIID))
            ->AddChatPal(szName, Data->Index, 0);
        iMoveOffset += sizeof(FS_CHAT_USERLIST_DATA);
    }
}

void SessionUiUnit::ReceiveChatRoomChatText(DWORD dwWindowUIID, const BYTE *ReceiveBuffer)
{
    auto Data = (LPFS_CHAT_TEXT)ReceiveBuffer;
    auto *pChatWindow = (CUIChatWindow *)g_pWindowMgr->GetWindow(dwWindowUIID);
    if (pChatWindow == NULL)
        return;

    char temp[MAX_CHATROOM_TEXT_LENGTH] = {};
    if (Data->MsgSize >= MAX_CHATROOM_TEXT_LENGTH)
        return;

    memcpy(temp, Data->Msg, Data->MsgSize);
    BuxConvert((LPBYTE)temp, Data->MsgSize);

    wchar_t chatMessage[MAX_CHATROOM_TEXT_LENGTH] = {};
    CMultiLanguage::ConvertFromUtf8(chatMessage, temp, MAX_CHATROOM_TEXT_LENGTH);

    if (pChatWindow->GetState() == UISTATE_READY)
    {
        pChatWindow->OriginatingSession().FriendMenuObject().SetNewChatAlert(dwWindowUIID);
        g_pSystemLogBox->AddText(I18N::Game::NewMessageHasArrived, SEASON3B::TYPE_SYSTEM_MESSAGE);
        pChatWindow->SetState(UISTATE_HIDE);
        if (g_pWindowMgr->GetFriendMainWindow() != NULL)
        {
            g_pWindowMgr->GetFriendMainWindow()->AddWindow(
                dwWindowUIID, g_pWindowMgr->GetWindow(dwWindowUIID)->GetTitle());
        }
    }
    else if (pChatWindow->GetState() == UISTATE_HIDE ||
             g_pWindowMgr->GetTopWindowUIID() != dwWindowUIID)
    {
        pChatWindow->OriginatingSession().FriendMenuObject().SetNewChatAlert(dwWindowUIID);
    }
    pChatWindow->AddChatText(Data->Index, chatMessage, 3, 0);
}

void SessionUiUnit::ReceiveChatRoomNoticeText(DWORD dwWindowUIID, const BYTE *ReceiveBuffer)
{
    auto Data = (LPFS_CHAT_TEXT)ReceiveBuffer;
    Data->Msg[99] = '\0';
    if (Data->Msg[0] == '\0')
    {
        return;
    }

    wchar_t message[sizeof Data->Msg]{};
    CMultiLanguage::ConvertFromUtf8(message, Data->Msg, sizeof Data->Msg);
    g_pSystemLogBox->AddText(message, SEASON3B::TYPE_SYSTEM_MESSAGE);
}

void SessionUiUnit::TranslateChattingProtocol(DWORD dwWindowUIID, const BYTE *ReceiveBuffer,
                                              int Size)
{
    if (Size < 4)
    {
        return;
    }

    int HeadCode;
    BOOL bIsC1C3 = ReceiveBuffer[0] % 2 == 1;
    if (bIsC1C3) // C1 and C3
    {
        HeadCode = ReceiveBuffer[2];
    }
    else
    {
        HeadCode = ReceiveBuffer[3];
    }

    switch (HeadCode)
    {
    case 0x00:
        ReceiveChatRoomConnectResult(dwWindowUIID, ReceiveBuffer);
        break;
    case 0x01:
        ReceiveChatRoomUserStateChange(dwWindowUIID, ReceiveBuffer);
        break;
    case 0x02:
        ReceiveChatRoomUserList(dwWindowUIID, ReceiveBuffer);
        break;
    case 0x04:
        ReceiveChatRoomChatText(dwWindowUIID, ReceiveBuffer);
        break;
    case 0x0D:
        ReceiveChatRoomNoticeText(dwWindowUIID, ReceiveBuffer);
        break;
    default:
        break;
    }
}

void SessionLegacyCalls::ReceiveChatRoomConnectResult(DWORD windowUiId, const BYTE *receiveBuffer)
{
    sessionKeeper_.Ui()->ReceiveChatRoomConnectResult(windowUiId, receiveBuffer);
}

void SessionLegacyCalls::ReceiveChatRoomUserStateChange(DWORD windowUiId, const BYTE *receiveBuffer)
{
    sessionKeeper_.Ui()->ReceiveChatRoomUserStateChange(windowUiId, receiveBuffer);
}

void SessionLegacyCalls::ReceiveChatRoomUserList(DWORD windowUiId, const BYTE *receiveBuffer)
{
    sessionKeeper_.Ui()->ReceiveChatRoomUserList(windowUiId, receiveBuffer);
}

void SessionLegacyCalls::ReceiveChatRoomChatText(DWORD windowUiId, const BYTE *receiveBuffer)
{
    sessionKeeper_.Ui()->ReceiveChatRoomChatText(windowUiId, receiveBuffer);
}

void SessionLegacyCalls::ReceiveChatRoomNoticeText(DWORD windowUiId, const BYTE *receiveBuffer)
{
    sessionKeeper_.Ui()->ReceiveChatRoomNoticeText(windowUiId, receiveBuffer);
}

void SessionLegacyCalls::TranslateChattingProtocol(DWORD windowUiId, const BYTE *receiveBuffer,
                                                   int size)
{
    sessionKeeper_.Ui()->TranslateChattingProtocol(windowUiId, receiveBuffer, size);
}

/*
void CChatRoomSocketList::ProcessSocketMessage(DWORD dwSocketID, WORD wMessage)
{
    CHATROOM_SOCKET * pChatroomSocket = GetChatRoomSocketData(GetChatRoomSocketID(dwSocketID));
    if (pChatroomSocket == NULL) return;
    Connection* pSocketClient = &pChatroomSocket->m_WSClient;

    if (pSocketClient == NULL)
    {
        return;
    }
    switch(wMessage)
    {
    case FD_CONNECT:
        break;
    case FD_READ :
        // pSocketClient->nRecv();
        break;
    case FD_WRITE :
        // pSocketClient->FDWriteSend();
        break;
    case FD_CLOSE :
        CUIChatWindow * pWindow = (CUIChatWindow *)g_pWindowMgr->GetWindow(pChatroomSocket->m_dwWindowUIID);
        if (pWindow != NULL)
            pWindow->AddChatText(255, I18N::Game::YouAreDisconnectedFromTheServer, 1, 0);
        pSocketClient->Close();
        break;
    }
}

//void CChatRoomSocketList::ProtocolCompile()
//{
//	// TODO: Change that
//	for (m_ChatRoomSocketMapIter = m_ChatRoomSocketMap.begin(); m_ChatRoomSocketMapIter != m_ChatRoomSocketMap.end(); ++m_ChatRoomSocketMapIter)
//	{
//		ProtocolCompiler(&m_ChatRoomSocketMapIter->second->m_WSClient, 1, m_ChatRoomSocketMapIter->second->m_dwWindowUIID);
//	}
//}
*/

void SessionLegacyCalls::SetLineColor(int type, float alphaRate)
{
    sessionKeeper_.Ui()->SetLineColor(type, alphaRate);
}

SEASON3B::CNewUIManager::CNewUIManager(SessionKeeper &keeper)
    : SessionUiLegacyBindings(keeper), sessionUi(UiForConstruction())
{
    m_pActiveMouseUIObj = NULL;
    m_pActiveKeyUIObj = NULL;
#ifdef PBG_MOD_STAMINA_UI
    m_nShowUICnt = 0;
#endif //PBG_MOD_STAMINA_UI
}

SEASON3B::CNewUIManager::~CNewUIManager()
{
    RemoveAllUIObjs();
}

void SEASON3B::CNewUIManager::AddUIObj(DWORD dwKey, CNewUIObj *pUIObj)
{
    auto mi = m_mapUI.find(dwKey);
    if (mi == m_mapUI.end())
    {
        m_vecUI.push_back(pUIObj);
        m_mapUI.insert(type_map_uibase::value_type(dwKey, pUIObj));
        MarkOrderDirty();
        BringToFront(pUIObj);
    }
}

void SEASON3B::CNewUIManager::RemoveUIObj(DWORD dwKey)
{
    auto mi = m_mapUI.find(dwKey);
    if (mi != m_mapUI.end())
    {
        auto vi = std::find(m_vecUI.begin(), m_vecUI.end(), (*mi).second);
        if (vi != m_vecUI.end())
        {
            m_vecUI.erase(vi);
        }
        MarkOrderDirty((*mi).second);
        m_mapUI.erase(mi);
    }
}

void SEASON3B::CNewUIManager::RemoveUIObj(CNewUIObj *pUIObj)
{
    auto mi = m_mapUI.begin();
    for (; mi != m_mapUI.end(); mi++)
    {
        if ((*mi).second == pUIObj)
        {
            m_mapUI.erase(mi);
            break;
        }
    }

    auto vi = std::find(m_vecUI.begin(), m_vecUI.end(), pUIObj);
    if (vi != m_vecUI.end())
    {
        m_vecUI.erase(vi);
        MarkOrderDirty(pUIObj);
    }
}

void SEASON3B::CNewUIManager::RemoveAllUIObjs()
{
#if defined(_DEBUG)

    {
        unsigned int uiUIManageCNT = m_mapUI.size();

        type_map_uibase::iterator mi = m_mapUI.begin();
        for (; mi != m_mapUI.end(); ++mi)
        {
            DWORD dwKey = (*mi).first;
            CNewUIObj *pUIObj = (*mi).second;
            if (pUIObj != NULL)
            {
                __TraceF(TEXT("UIKEY(%d) : mapUI \n"), uiUIManageCNT, dwKey);
            }
        }

        type_vector_uibase::iterator vi = m_vecUI.begin();
        for (; vi < m_vecUI.end(); ++vi)
        {
            CNewUIObj *pUIObj = (*vi);
            if (pUIObj != NULL)
            {
                __TraceF(TEXT("vecUI \n"), uiUIManageCNT);
            }
        }
    }

#endif // defined(_DEBUG)
    std::fill(m_layerOrderedUI.begin(), m_layerOrderedUI.end(), nullptr);
    std::fill(m_keyOrderedUI.begin(), m_keyOrderedUI.end(), nullptr);
    m_vecUI.clear();
    m_mapUI.clear();
    m_orderDirty = true;
    m_nextDynamicLayerOrder = 0;
}

void SEASON3B::CNewUIManager::MarkOrderDirty(CNewUIObj *removed) noexcept
{
    m_orderDirty = true;
    if (removed == nullptr)
    {
        return;
    }
    std::replace(m_layerOrderedUI.begin(), m_layerOrderedUI.end(), removed,
                 static_cast<CNewUIObj *>(nullptr));
    std::replace(m_keyOrderedUI.begin(), m_keyOrderedUI.end(), removed,
                 static_cast<CNewUIObj *>(nullptr));
}

void SEASON3B::CNewUIManager::RefreshOrderedUI()
{
    if (!m_orderDirty)
    {
        return;
    }

    m_layerOrderedUI = m_vecUI;
    std::sort(m_layerOrderedUI.begin(), m_layerOrderedUI.end(),
              [this](auto *left, auto *right) { return CompareLayerDepth(left, right); });
    m_keyOrderedUI = m_vecUI;
    std::sort(m_keyOrderedUI.begin(), m_keyOrderedUI.end(), CompareKeyEventOrder);
    m_orderDirty = false;
}

CNewUIObj *SEASON3B::CNewUIManager::FindUIObj(DWORD dwKey)
{
    auto mi = m_mapUI.find(dwKey);
    if (mi != m_mapUI.end())
        return (*mi).second;
    return NULL;
}

bool SEASON3B::CNewUIManager::UpdateMouseEvent()
{
    m_pActiveMouseUIObj = NULL;
    RefreshOrderedUI();

    for (auto vi = m_layerOrderedUI.rbegin(); vi != m_layerOrderedUI.rend(); ++vi)
    {
        CNewUIObj *const ui = *vi;
        if (ui != nullptr && ui->IsVisible())
        {
            const bool bResult = ui->UpdateMouseEvent();
            if (bResult == false)
            {
                if (*vi == ui)
                {
                    m_pActiveMouseUIObj = ui;
                    if (IsPress(VK_LBUTTON))
                        BringToFront(ui);
                }
                return false;
            }
        }
    }

    return true;
}

bool SEASON3B::CNewUIManager::UpdateKeyEvent(CNewUIHotKey &hotKey)
{
    m_pActiveKeyUIObj = NULL;
    RefreshOrderedUI();

    // Portable legacy fields use their stable related handle. Retained fields
    // publish their owning UI object, so only that object receives polled keys.
    CUITextInputBox *pFocusedField = focusedTextInputBox_;
    const HWND hFocus = pFocusedField ? reinterpret_cast<HWND>(pFocusedField) : GetFocus();
    CNewUIObj *const focusedModernUi = sessionUi.FocusedModernUiObject();

    for (auto vi = m_keyOrderedUI.begin(); vi != m_keyOrderedUI.end(); ++vi)
    {
        CNewUIObj *const ui = *vi;
        if (ui == nullptr)
        {
            continue;
        }
        if (focusedModernUi != nullptr && ui != focusedModernUi)
        {
            continue;
        }
        HWND hRelatedWnd = ui->GetRelatedWnd();
        if (NULL == hRelatedWnd)
        {
            hRelatedWnd = g_hWnd;
        }

        HWND hWnd = hFocus;

        if (ui->IsEnabled() && hWnd == hRelatedWnd)
        {
            const bool updated = ui == &hotKey ? hotKey.UpdateKeyEvent() : ui->UpdateKeyEvent();
            if (false == updated)
            {
                if (*vi == ui)
                {
                    m_pActiveKeyUIObj = ui;
                }
                return false; //. stop calling UpdateKeyEvent functions
            }
        }
    }
    return true;
}

bool SEASON3B::CNewUIManager::Update()
{
    RefreshOrderedUI();

    for (auto vi = m_layerOrderedUI.begin(); vi != m_layerOrderedUI.end(); ++vi)
    {
        CNewUIObj *const ui = *vi;
        if (ui != nullptr && ui->IsEnabled())
        {
            if (false == ui->Update())
            {
                return false; //. stop calling Update functions
            }
        }
    }

    return true;
}

CNewUIObj *SEASON3B::CNewUIManager::GetActiveMouseUIObj()
{
    return m_pActiveMouseUIObj;
}

CNewUIObj *SEASON3B::CNewUIManager::GetActiveKeyUIObj()
{
    return m_pActiveKeyUIObj;
}

void SEASON3B::CNewUIManager::ResetActiveUIObj()
{
    m_pActiveMouseUIObj = NULL;
    m_pActiveKeyUIObj = NULL;
}

void SEASON3B::CNewUIManager::BringToFront(CNewUIObj *ui)
{
    using namespace UI::Modern::MigratedUiRenderLayers;
    if (ui == nullptr || !IsDynamic(ui->GetLayerDepth()))
        return;
    ui->SetDynamicLayerOrder(++m_nextDynamicLayerOrder);
    MarkOrderDirty();
}

bool SEASON3B::CNewUIManager::IsInterfaceVisible(DWORD dwKey)
{
    CNewUIObj *pObj = FindUIObj(dwKey);
    if (NULL == pObj)
    {
        return false;
    }
    return pObj->IsVisible();
}

bool SEASON3B::CNewUIManager::IsInterfaceEnabled(DWORD dwKey)
{
    CNewUIObj *pObj = FindUIObj(dwKey);
    if (NULL == pObj)
        return false;
    return pObj->IsEnabled();
}

void SEASON3B::CNewUIManager::ShowInterface(DWORD dwKey, bool bShow /* = true*/)
{
    CNewUIObj *pObj = FindUIObj(dwKey);
    if (NULL != pObj)
        pObj->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableInterface(DWORD dwKey, bool bEnable /* = true*/)
{
    CNewUIObj *pObj = FindUIObj(dwKey);
    if (NULL != pObj)
        pObj->Enable(bEnable);
}

void SEASON3B::CNewUIManager::ShowAllInterfaces(bool bShow /* = true*/)
{
    auto mi = m_mapUI.begin();
    for (; mi != m_mapUI.end(); mi++)
        (*mi).second->Show(bShow);
}

void SEASON3B::CNewUIManager::EnableAllInterfaces(bool bEnable /* = true*/)
{
    auto mi = m_mapUI.begin();
    for (; mi != m_mapUI.end(); mi++)
        (*mi).second->Show(bEnable);
}

bool SEASON3B::CNewUIManager::CompareLayerDepth(CNewUIObj *left, CNewUIObj *right) const
{
    return UI::Modern::MigratedUiRenderLayers::IsBefore(
        left->GetLayerDepth(), left->GetDynamicLayerOrder(), right->GetLayerDepth(),
        right->GetDynamicLayerOrder());
}

bool SEASON3B::CNewUIManager::CompareKeyEventOrder(INewUIBase *pObj1, INewUIBase *pObj2)
{
    return pObj1->GetKeyEventOrder() > pObj2->GetKeyEventOrder();
}

#ifdef PBG_MOD_STAMINA_UI
int SEASON3B::CNewUIManager::GetShowUICnt()
{
    int m_nShowUICnt = 0;
    // How many of certain interfaces are open
    for (int i = INTERFACE_PARTY; i < INTERFACE_CHARACTER + 1; ++i)
    {
        if (IsInterfaceVisible(i))
            m_nShowUICnt++;
    }
    return m_nShowUICnt;
}
#endif //PBG_MOD_STAMINA_UI

namespace
{

bool IsHeroPositionLayoutInterface(DWORD dwKey)
{
    switch (dwKey)
    {
    case SEASON3B::INTERFACE_INVENTORY:
    case SEASON3B::INTERFACE_INVENTORY_EXT:
    case SEASON3B::INTERFACE_STORAGE:
    case SEASON3B::INTERFACE_STORAGE_EXT:
    case SEASON3B::INTERFACE_CHARACTER:
    case SEASON3B::INTERFACE_NPCSHOP:
    case SEASON3B::INTERFACE_MIXINVENTORY:
    case SEASON3B::INTERFACE_TRADE:
    case SEASON3B::INTERFACE_MYSHOP_INVENTORY:
    case SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY:
        return true;
    default:
        return false;
    }
}
} // namespace

bool SEASON3B::CNewUISystem::CreatePersonalItemTable()
{
    return SessionUiLegacyBindings::CreatePersonalItemTable();
}

void SEASON3B::CNewUISystem::ReleasePersonalItemTable()
{
    SessionUiLegacyBindings::ReleasePersonalItemTable();
}

CNewUISystem::CNewUISystem(SessionKeeper &keeper) : SessionUiLegacyBindings(keeper)

{
    g_pNewUISystem = this;
    m_pNewUIMng = nullptr;
    m_pNewUI3DRenderMng = nullptr;
    m_pNewUIHotKey = nullptr;
    m_pNewChatLogWindow = nullptr;
    m_pNewSystemLogWindow = nullptr;
    m_pNewSlideWindow = nullptr;
    m_pNewGuildMakeWindow = nullptr;
    m_pNewFriendWindow = nullptr;
    m_pNewMainFrameWindow = nullptr;
    m_pNewSkillList = nullptr;
    m_pNewChatInputBox = nullptr;
    m_pNewItemMng = nullptr;
    m_pNewMyInventory = nullptr;
    m_pNewMyInventoryExt = nullptr;
    m_pNewNPCShop = nullptr;
    m_pNewPetInfoWindow = nullptr;
    m_pNewMixInventory = nullptr;
    m_pNewCastleWindow = nullptr;
    m_pNewGuardWindow = nullptr;
    m_pNewGatemanWindow = nullptr;
    m_pNewGateSwitchWindow = nullptr;
    m_pNewStorageInventory = nullptr;
    m_pNewStorageInventoryExt = nullptr;
    m_pNewGuildInfoWindow = nullptr;
    m_pNewMyShopInventory = nullptr;
    m_pNewPurchaseShopInventory = nullptr;
    m_pNewCharacterInfoWindow = nullptr;
    m_pNewMyQuestInfoWindow = nullptr;
    m_pNewPartyListWindow = nullptr;
    m_pNewNPCQuest = nullptr;
    m_pNewEnterBloodCastle = nullptr;
    m_pNewEnterDevilSquare = nullptr;
    m_pNewBloodCastle = nullptr;
    m_pNewTrade = nullptr;
    m_pNewKanturu2ndEnterNpc = nullptr;
    m_pNewKanturuInfoWindow = nullptr;
    m_pNewCatapultWindow = nullptr;
    m_pNewChaosCastleTime = nullptr;
    m_pNewBattleSoccerScore = nullptr;
    m_pNewCommandWindow = nullptr;
    m_pNewWindowMenu = nullptr;
    m_pNewOptionWindow = nullptr;
    m_pNewHeroPositionInfo = nullptr;
    m_pNewHelpWindow = nullptr;
    m_pNewItemExplanationWindow = nullptr;
    m_pNewSetItemExplanation = nullptr;
    m_pNewQuickCommandWindow = nullptr;
    m_pNewMoveCommandWindow = nullptr;
    m_pNewDuelWindow = nullptr;
    m_pNewNameWindow = nullptr;
    m_pNewSiegeWarfare = nullptr;
    m_pNewItemEnduranceInfo = nullptr;
    m_pNewBuffWindow = nullptr;
    m_pNewCryWolfInterface = nullptr;
    m_pNewMaster_Level_Interface = nullptr;
    m_pNewCursedTempleResultWindow = nullptr;
    m_pNewCursedTempleWindow = nullptr;
    m_pNewCursedTempleEnterWindow = nullptr;
    m_pNewGoldBowman = nullptr;
    m_pNewGoldBowmanLena = nullptr;
    m_pNewLuckyCoinRegistration = nullptr;
    m_pNewExchangeLuckyCoinWindow = nullptr;
    m_pNewDuelWatchWindow = nullptr;
    m_pNewDuelWatchMainFrameWindow = nullptr;
    m_pNewDuelWatchUserListWindow = nullptr;
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    m_pNewInGameShop = nullptr;
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME
    m_pNewDoppelGangerWindow = nullptr;
    m_pNewDoppelGangerFrame = nullptr;
    m_pNewNPCDialogue = nullptr;
    m_pNewQuestProgress = nullptr;
    m_pNewQuestProgressByEtc = nullptr;
    m_pNewEmpireGuardianNPC = nullptr;
    m_pNewEmpireGuardianTimer = nullptr;
    m_pNewMiniMap = nullptr;
#ifdef PBG_MOD_STAMINA_UI
    m_pNewUIStamina = NULL;
#endif //PBG_MOD_STAMINA_UI
#ifdef PBG_ADD_GENSRANKING
    m_pNewGensRanking = nullptr;
#endif //PBG_ADD_GENSRANKING
    m_pNewUnitedMarketPlaceWindow = nullptr;
    m_pNewUILuckyItemWnd = nullptr;
    m_pNewUIMuHelper = nullptr;
    m_pNewUIMuHelperSkillList = nullptr;
}

CNewUISystem::~CNewUISystem()
{
    Release();
    if (g_pNewUISystem == this)
    {
        g_pNewUISystem = nullptr;
    }
}

bool CNewUISystem::Create()
{
    SessionKeeper &keeper = SessionOrigin();
    m_pNewUIMng = new CNewUIManager(keeper);

    m_pNewUI3DRenderMng = new CNewUI3DRenderMng(keeper);
    if (false == m_pNewUI3DRenderMng->Create(m_pNewUIMng))
        return false;

    m_pNewChatLogWindow = new CNewUIChatLogWindow(keeper);
    if (false == m_pNewChatLogWindow->Create(m_pNewUIMng, 0, 480 - 50 - 47, 6))
        return false;

    m_pNewSystemLogWindow = new CNewUISystemLogWindow(keeper);
    if (false == m_pNewSystemLogWindow->Create(m_pNewUIMng, m_pNewChatLogWindow, 0, 80))
        return false;

    m_pNewOptionWindow = new CNewUIOptionWindow(keeper);
    if (m_pNewOptionWindow->Create(m_pNewUIMng, (640 / 2) - (190 / 2), 5) == false)
    {
        return false;
    }

    m_pNewSlideWindow = new CNewUISlideWindow(keeper);
    if (m_pNewSlideWindow->Create(m_pNewUIMng) == false)
    {
        return false;
    }

    // OpenBasicData loads the per-session master-skill tables before the
    // main-scene interfaces are registered. Construct their owner now and
    // keep Create() at the existing main-scene load point below.
    m_pNewMaster_Level_Interface = new CNewUIMasterLevel(keeper);

    return true;
}

bool SessionUiUnit::InitializeLegacyUi()
{
    legacyUiInitialized_ = g_pNewUISystem->Create();
    if (!legacyUiInitialized_)
    {
        g_pNewUISystem->Release();
        return false;
    }

    if (!g_MessageBox.Create(g_pNewUISystem->GetNewUIManager()))
    {
        g_pNewUISystem->Release();
        legacyUiInitialized_ = false;
        return false;
    }

    InitializeLegacyManager();
    return true;
}

void SessionUiUnit::UpdateResolutionDependentSystems()
{
    if (!legacyUiInitialized_)
    {
        return;
    }
    if (g_pNewUI3DRenderMng != nullptr)
    {
        g_pNewUI3DRenderMng->UpdateAllCameraDimensions(WindowWidth, WindowHeight);
    }
    if (CNewUIMyInventory *inventory = g_pNewUISystem->GetUI_NewMyInventory())
    {
        inventory->SetEquipmentSlotInfo();
    }
    LegacyUiManager().RepositionSceneUI();
}

void SessionUiUnit::ShutdownLegacyUi() noexcept
{
    if (!legacyUiInitialized_)
    {
        return;
    }

    g_MessageBox.Release();
    g_pNewUISystem->Release();
    ShutdownLegacyManager();
    legacyUiInitialized_ = false;
}

void CNewUISystem::Release()
{
    if (m_pNewUIMng == nullptr)
    {
        return;
    }

    UnloadMainSceneInterface();

    SAFE_DELETE(m_pNewSlideWindow);
    SAFE_DELETE(m_pNewOptionWindow);
    SAFE_DELETE(m_pNewChatLogWindow);
    SAFE_DELETE(m_pNewSystemLogWindow);
    SAFE_DELETE(m_pNewUI3DRenderMng);

    m_pNewUIMng->RemoveAllUIObjs();

    SAFE_DELETE(m_pNewUIMng);
}

bool CNewUISystem::LoadMainSceneInterface()
{
    SessionKeeper &keeper = SessionOrigin();

    m_pNewChatLogWindow->Show(true);
    m_pNewSystemLogWindow->Show(true);
    m_pNewSlideWindow->Show(true);

    m_pNewItemMng = &keeper.GameData()->Items();

    m_pNewChatInputBox = new CNewUIChatInputBox(keeper);

    if (false == m_pNewChatInputBox->Create(m_pNewUIMng, m_pNewChatLogWindow, m_pNewSystemLogWindow,
                                            0, 480 - 51 - 47))
    {
        return false;
    }

    SetFocus(keeper.PlatformWindowHandle());

    m_pNewUIHotKey = new CNewUIHotKey(keeper);
    if (false == m_pNewUIHotKey->Create(m_pNewUIMng))
        return false;

    m_pNewMainFrameWindow = new CNewUIMainFrameWindow(keeper);
    if (m_pNewMainFrameWindow->Create(m_pNewUIMng, m_pNewUI3DRenderMng) == false)
        return false;

    m_pNewSkillList = new CNewUISkillList(keeper);
    if (m_pNewSkillList->Create(m_pNewUIMng, m_pNewUI3DRenderMng) == false)
        return false;

    m_pNewFriendWindow = new CNewUIFriendWindow(keeper);
    if (m_pNewFriendWindow->Create(m_pNewUIMng) == false)
        return false;

    m_pNewMyInventory = new CNewUIMyInventory(keeper);
    if (false == m_pNewMyInventory->Create(m_pNewUIMng, m_pNewUI3DRenderMng,
                                           UiSystemDetail::PanelColumnX(1), 0))
        return false;

    m_pNewMyInventoryExt = new CNewUIInventoryExtension(keeper);
    if (false == m_pNewMyInventoryExt->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0))
        return false;

    m_pNewNPCShop = new CNewUINPCShop(keeper);
    if (false == m_pNewNPCShop->Create(m_pNewUIMng, CNewUINPCShop::NPCSHOP_POS_X,
                                       CNewUINPCShop::NPCSHOP_POS_Y))
        return false;

    m_pNewPetInfoWindow = new CNewUIPetInfoWindow(keeper);
    if (false == m_pNewPetInfoWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0))
        return false;

    m_pNewMixInventory = new CNewUIMixInventory(keeper);
    if (m_pNewMixInventory->Create(m_pNewUIMng, 260, 0) == false)
        return false;

    m_pNewCastleWindow = new CNewUICastleWindow(keeper);
    if (m_pNewCastleWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewGuardWindow = new CNewUIGuardWindow(keeper);
    if (m_pNewGuardWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewGatemanWindow = new CNewUIGatemanWindow(keeper);
    if (m_pNewGatemanWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewGateSwitchWindow = new CNewUIGateSwitchWindow(keeper);
    if (m_pNewGateSwitchWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewStorageInventory = new CNewUIStorageInventory(keeper);
    if (m_pNewStorageInventory->Create(m_pNewUIMng, 260, 0) == false)
        return false;

    m_pNewStorageInventoryExt = new CNewUIStorageInventoryExt(keeper);
    if (m_pNewStorageInventoryExt->Create(m_pNewUIMng, 260 - 190, 0) == false)
        return false;

    m_pNewGuildInfoWindow = new CNewUIGuildInfoWindow(keeper);
    if (m_pNewGuildInfoWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewGuildMakeWindow = new CNewUIGuildMakeWindow(keeper);
    if (m_pNewGuildMakeWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    CreatePersonalItemTable();

    m_pNewMyShopInventory = new CNewUIMyShopInventory(keeper);
    if (m_pNewMyShopInventory->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0) == false)
        return false;

    m_pNewPurchaseShopInventory = new CNewUIPurchaseShopInventory(keeper);
    if (m_pNewPurchaseShopInventory->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0) ==
        false)
        return false;

    m_pNewCharacterInfoWindow = new CNewUICharacterInfoWindow(keeper);
    if (m_pNewCharacterInfoWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewMyQuestInfoWindow = new CNewUIMyQuestInfoWindow(keeper);
    if (m_pNewMyQuestInfoWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewPartyListWindow = new CNewUIPartyListWindow(keeper);
    if (m_pNewPartyListWindow->Create(m_pNewUIMng, 640 - 79, 14) == false)
        return false;

    m_pNewNPCQuest = new CNewUINPCQuest(keeper);
    if (m_pNewNPCQuest->Create(m_pNewUIMng, m_pNewUI3DRenderMng, UiSystemDetail::PanelColumnX(1),
                               0) == false)
        return false;

    m_pNewEnterBloodCastle = new CNewUIEnterBloodCastle(keeper);
    if (m_pNewEnterBloodCastle->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewEnterDevilSquare = new CNewUIEnterDevilSquare(keeper);
    if (m_pNewEnterDevilSquare->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewBloodCastle = new CNewUIBloodCastle(keeper);
    if (m_pNewBloodCastle->Create(m_pNewUIMng, 640 - 127, 480 - 132) == false)
        return false;

    m_pNewTrade = new CNewUITrade(keeper);
    if (m_pNewTrade->Create(m_pNewUIMng, 260, 0) == false)
        return false;

    m_pNewKanturu2ndEnterNpc = new CNewUIKanturu2ndEnterNpc(keeper);
    if (m_pNewKanturu2ndEnterNpc->Create(m_pNewUIMng, (640 / 2) - (230 / 2), 20) == false)
    {
        return false;
    }

    m_pNewKanturuInfoWindow = new CNewUIKanturuInfoWindow(keeper);
    if (m_pNewKanturuInfoWindow->Create(m_pNewUIMng, 541, 351) == false)
    {
        return false;
    }

    m_pNewChaosCastleTime = new CNewUIChaosCastleTime(keeper);
    if (m_pNewChaosCastleTime->Create(m_pNewUIMng, 640 - 127, 480 - 132) == false)
        return false;

    m_pNewBattleSoccerScore = new CNewUIBattleSoccerScore(keeper);
    if (m_pNewBattleSoccerScore->Create(m_pNewUIMng, 509, 359) == false)
        return false;

    m_pNewCommandWindow = new CNewUICommandWindow(keeper);
    if (m_pNewCommandWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewCatapultWindow = new CNewUICatapultWindow(keeper);
    if (m_pNewCatapultWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
    {
        return false;
    }

    m_pNewWindowMenu = new CNewUIWindowMenu(keeper);
    if (m_pNewWindowMenu->Create(m_pNewUIMng, 640 - 112, 480 - 171) == false)
    {
        return false;
    }

    m_pNewHeroPositionInfo = new CNewUIHeroPositionInfo(keeper);
    if (m_pNewHeroPositionInfo->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewHelpWindow = new CNewUIHelpWindow(keeper);
    if (m_pNewHelpWindow->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewItemExplanationWindow = new CNewUIItemExplanationWindow(keeper);
    if (m_pNewItemExplanationWindow->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewSetItemExplanation = new CNewUISetItemExplanation(keeper);
    if (m_pNewSetItemExplanation->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewQuickCommandWindow = new CNewUIQuickCommandWindow(keeper);
    if (m_pNewQuickCommandWindow->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewMoveCommandWindow = new CNewUIMoveCommandWindow(keeper);

    if (m_pNewMoveCommandWindow->Create(m_pNewUIMng, 1, 1) == false)
        return false;

    m_pNewDuelWindow = new CNewUIDuelWindow(keeper);
    if (m_pNewDuelWindow->Create(m_pNewUIMng, 509, 359) == false)
    {
        return false;
    }

    m_pNewNameWindow = new CNewUINameWindow(keeper);
    if (m_pNewNameWindow->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }

    m_pNewSiegeWarfare = new CNewUISiegeWarfare(keeper);
    if (m_pNewSiegeWarfare->Create(m_pNewUIMng, 486, 234) == false)
        return false;

    m_pNewItemEnduranceInfo = new CNewUIItemEnduranceInfo(keeper);
    if (m_pNewItemEnduranceInfo->Create(m_pNewUIMng, 2, 26) == false)
    {
        return false;
    }

    m_pNewBuffWindow = new CNewUIBuffWindow(keeper);
    if (m_pNewBuffWindow->Create(m_pNewUIMng, 220, 15) == false)
    {
        return false;
    }

    m_pNewCursedTempleEnterWindow = new CNewUICursedTempleEnter(keeper);
    if (m_pNewCursedTempleEnterWindow->Create(m_pNewUIMng, 640 / 2 - 230 / 2, 80) == false)
    {
        return false;
    }
    m_pNewCursedTempleWindow = new CNewUICursedTempleSystem(keeper);
    if (m_pNewCursedTempleWindow->Create(m_pNewUIMng, 0, 0) == false)
    {
        return false;
    }
    m_pNewCursedTempleResultWindow = new CNewUICursedTempleResult(keeper);
    if (m_pNewCursedTempleResultWindow->Create(m_pNewUIMng, 640 / 2 - 230 / 2, 120) == false)
    {
        return false;
    }

    m_pNewCryWolfInterface = new CNewUICryWolf(keeper);
    if (m_pNewCryWolfInterface->Create(m_pNewUIMng, 0, 0) == false)
        return false;

    if (m_pNewMaster_Level_Interface == nullptr ||
        m_pNewMaster_Level_Interface->Create(m_pNewUIMng) == false)
        return false;

    m_pNewMiniMap = new CNewUIMiniMap(keeper);
    if (m_pNewMiniMap->Create(m_pNewUIMng, 0, 0) == false)
        return false;

    m_pNewGoldBowman = new CNewUIGoldBowmanWindow(keeper);
    if (m_pNewGoldBowman->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewGoldBowmanLena = new CNewUIGoldBowmanLena(keeper);
    if (m_pNewGoldBowmanLena->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewLuckyCoinRegistration = new CNewUIRegistrationLuckyCoin(keeper);
    if (m_pNewLuckyCoinRegistration->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0) ==
        false)
        return false;

    m_pNewExchangeLuckyCoinWindow = new CNewUIExchangeLuckyCoin(keeper);
    if (m_pNewExchangeLuckyCoinWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(2), 0) ==
        false)
        return false;

    m_pNewDuelWatchWindow = new CNewUIDuelWatchWindow(keeper);
    if (m_pNewDuelWatchWindow->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewDuelWatchMainFrameWindow = new CNewUIDuelWatchMainFrameWindow(keeper);
    if (m_pNewDuelWatchMainFrameWindow->Create(m_pNewUIMng, m_pNewUI3DRenderMng) == false)
        return false;

    m_pNewDuelWatchUserListWindow = new CNewUIDuelWatchUserListWindow(keeper);
    if (m_pNewDuelWatchUserListWindow->Create(m_pNewUIMng, 640 - 57, 480 - 51) == false)
        return false;

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    m_pNewInGameShop = new CNewUIInGameShop(keeper);
    if (m_pNewInGameShop->Create(m_pNewUIMng, 0, 0) == false)
        return false;
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME

    m_pNewDoppelGangerWindow = new CNewUIDoppelGangerWindow(keeper);
    if (m_pNewDoppelGangerWindow->Create(m_pNewUIMng, m_pNewUI3DRenderMng,
                                         UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewDoppelGangerFrame = new CNewUIDoppelGangerFrame(keeper);
    if (m_pNewDoppelGangerFrame->Create(m_pNewUIMng, 640 - 227, 480 - 51 - 87) == false)
        return false;

    m_pNewNPCDialogue = new CNewUINPCDialogue(keeper);
    if (m_pNewNPCDialogue->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewQuestProgress = new CNewUIQuestProgress(keeper);
    if (m_pNewQuestProgress->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewQuestProgressByEtc = new CNewUIQuestProgressByEtc(keeper);
    if (m_pNewQuestProgressByEtc->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewEmpireGuardianNPC = new CNewUIEmpireGuardianNPC(keeper);
    if (m_pNewEmpireGuardianNPC->Create(m_pNewUIMng, m_pNewUI3DRenderMng, 450, 0) == false)
        return false;

    m_pNewEmpireGuardianTimer = new CNewUIEmpireGuardianTimer(keeper);
    if (m_pNewEmpireGuardianTimer->Create(m_pNewUIMng, 507, 342) == false)
        return false;

#ifdef PBG_MOD_STAMINA_UI
    m_pNewUIStamina = new CNewUIStamina;
    if (m_pNewUIStamina->Create(m_pNewUIMng, 640, 480) == false)
        return false;
#endif //PBG_MOD_STAMINA_UI

    m_pNewGensRanking = new CNewUIGensRanking(keeper);
    if (m_pNewGensRanking->Create(m_pNewUIMng, 640, 480) == false)
        return false;

    m_pNewUnitedMarketPlaceWindow = new CNewUIUnitedMarketPlaceWindow(keeper);
    if (m_pNewUnitedMarketPlaceWindow->Create(m_pNewUIMng, m_pNewUI3DRenderMng,
                                              UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewUILuckyItemWnd = new CNewUILuckyItemWnd(keeper);
    if (m_pNewUILuckyItemWnd->Create(m_pNewUIMng, 260, 0) == false)
        return false;

    m_pNewUIMuHelper = new CNewUIMuHelper(keeper);
    if (m_pNewUIMuHelper->Create(m_pNewUIMng, UiSystemDetail::PanelColumnX(1), 0) == false)
        return false;

    m_pNewUIMuHelperSkillList = new CNewUIMuHelperSkillList(keeper);
    if (m_pNewUIMuHelperSkillList->Create(m_pNewUIMng) == false)
        return false;

    return true;
}

void CNewUISystem::UnloadMainSceneInterface()
{
    if (g_pNewUIMng)
    {
        g_pNewUIMng->ShowAllInterfaces(false);
    }

    if (m_pNewMyInventory != nullptr)
    {
        m_pNewMyInventory->GetInventoryCtrl()->ReleasePickedItemView();
    }

    SAFE_DELETE(m_pNewHelpWindow);
    SAFE_DELETE(m_pNewItemExplanationWindow);
    SAFE_DELETE(m_pNewSetItemExplanation);
    SAFE_DELETE(m_pNewQuickCommandWindow);
    SAFE_DELETE(m_pNewWindowMenu);
    SAFE_DELETE(m_pNewBattleSoccerScore);
    SAFE_DELETE(m_pNewCatapultWindow);
    SAFE_DELETE(m_pNewKanturu2ndEnterNpc);
    SAFE_DELETE(m_pNewKanturuInfoWindow);
    SAFE_DELETE(m_pNewTrade);
    SAFE_DELETE(m_pNewNPCQuest);
    SAFE_DELETE(m_pNewMyQuestInfoWindow);
    SAFE_DELETE(m_pNewCharacterInfoWindow);
    SAFE_DELETE(m_pNewPurchaseShopInventory);
    SAFE_DELETE(m_pNewMyShopInventory);
    SAFE_DELETE(m_pNewGuildMakeWindow);
    SAFE_DELETE(m_pNewGuildInfoWindow);
    SAFE_DELETE(m_pNewStorageInventory);
    SAFE_DELETE(m_pNewMixInventory);
    SAFE_DELETE(m_pNewCastleWindow);
    SAFE_DELETE(m_pNewGuardWindow);
    SAFE_DELETE(m_pNewGatemanWindow);
    SAFE_DELETE(m_pNewGateSwitchWindow);
    SAFE_DELETE(m_pNewNPCShop);
    SAFE_DELETE(m_pNewPetInfoWindow);
    SAFE_DELETE(m_pNewMyInventory);
    SAFE_DELETE(m_pNewFriendWindow);
    SAFE_DELETE(m_pNewChatInputBox);
    SAFE_DELETE(m_pNewNameWindow);
    SAFE_DELETE(m_pNewSkillList);
    SAFE_DELETE(m_pNewMainFrameWindow);
    SAFE_DELETE(m_pNewPartyListWindow);
    SAFE_DELETE(m_pNewEnterBloodCastle);
    SAFE_DELETE(m_pNewEnterDevilSquare);
    SAFE_DELETE(m_pNewBloodCastle);
    SAFE_DELETE(m_pNewChaosCastleTime);
    SAFE_DELETE(m_pNewCommandWindow);
    SAFE_DELETE(m_pNewHeroPositionInfo);
    SAFE_DELETE(m_pNewMoveCommandWindow);
    SAFE_DELETE(m_pNewUIHotKey);
    SAFE_DELETE(m_pNewSiegeWarfare);
    SAFE_DELETE(m_pNewItemEnduranceInfo);
    SAFE_DELETE(m_pNewBuffWindow);
    SAFE_DELETE(m_pNewCursedTempleResultWindow);
    SAFE_DELETE(m_pNewCursedTempleWindow);
    SAFE_DELETE(m_pNewCursedTempleEnterWindow);
    SAFE_DELETE(m_pNewCryWolfInterface);
    SAFE_DELETE(m_pNewMaster_Level_Interface);
    SAFE_DELETE(m_pNewGoldBowman);
    SAFE_DELETE(m_pNewGoldBowmanLena);
    SAFE_DELETE(m_pNewLuckyCoinRegistration);
    SAFE_DELETE(m_pNewExchangeLuckyCoinWindow);
    SAFE_DELETE(m_pNewDuelWatchWindow);
    SAFE_DELETE(m_pNewDuelWindow);
    SAFE_DELETE(m_pNewDuelWatchMainFrameWindow);
    SAFE_DELETE(m_pNewDuelWatchUserListWindow);
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    SAFE_DELETE(m_pNewInGameShop);
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME
    SAFE_DELETE(m_pNewDoppelGangerWindow);
    SAFE_DELETE(m_pNewDoppelGangerFrame);
    SAFE_DELETE(m_pNewNPCDialogue);
    SAFE_DELETE(m_pNewQuestProgress);
    SAFE_DELETE(m_pNewQuestProgressByEtc);
    SAFE_DELETE(m_pNewEmpireGuardianNPC);
    SAFE_DELETE(m_pNewEmpireGuardianTimer);
    SAFE_DELETE(m_pNewMiniMap);
    m_pNewItemMng = nullptr;
#ifdef PBG_MOD_STAMINA_UI
    SAFE_DELETE(m_pNewUIStamina);
#endif //PBG_MOD_STAMINA_UI
    SAFE_DELETE(m_pNewGensRanking);
    SAFE_DELETE(m_pNewUnitedMarketPlaceWindow);
#ifdef LEM_FIX_LUCKYITEM_UICLASS_SAFEDELETE
    SAFE_DELETE(m_pNewUILuckyItemWnd);
#endif // LEM_FIX_LUCKYITEM_UICLASS_SAFEDELETE
}

bool CNewUISystem::IsVisible(DWORD dwKey)
{
    if (m_pNewUIMng)
    {
        return m_pNewUIMng->IsInterfaceVisible(dwKey);
    }

    return false;
}

//bool SortUiObj(const INewUIBase& lhs, const INewUIBase& rhs)
//{
//	return lhs.GetDisplayOrder() > rhs.GetDisplayOrder();
//}

void CNewUISystem::Toggle(DWORD dwKey)
{
    IsVisible(dwKey) ? Hide(dwKey) : Show(dwKey);
}

void CNewUISystem::HideAll()
{
    if (m_pNewUIMng)
    {
        for (int i = INTERFACE_BEGIN + 1; i < INTERFACE_END; i++)
        {
            if (IsImpossibleHideInterface(i) == false)
            {
                if (IsVisible(i) == true)
                {
                    Hide(i);
                }
            }
        }
    }
}

void CNewUISystem::HideAllGroupA()
{
    Hide(INTERFACE_INVENTORY);
    Hide(INTERFACE_CHARACTER);

    DWORD dwGroupA[] = {
        //SEASON3B::INTERFACE_INVENTORY,
        //SEASON3B::INTERFACE_CHARACTER,
        //SEASON3B::INTERFACE_WINDOW_MENU,
        INTERFACE_MUHELPER,
        INTERFACE_MUHELPER_SKILL_LIST,
        INTERFACE_MIXINVENTORY,
        INTERFACE_STORAGE,
        INTERFACE_NPCSHOP,
        INTERFACE_MYSHOP_INVENTORY,
        INTERFACE_PURCHASESHOP_INVENTORY,
        INTERFACE_PET,
        INTERFACE_MYQUEST,
        INTERFACE_NPCQUEST,
        INTERFACE_SENATUS,
        INTERFACE_GUARDSMAN,
        INTERFACE_COMMAND,
        INTERFACE_GUILDINFO,
        INTERFACE_KANTURU2ND_ENTERNPC,
        INTERFACE_DUELWATCH,
        INTERFACE_DOPPELGANGER_NPC,
        //SEASON3B::INTERFACE_HELP,
        //SEASON3B::INTERFACE_ITEM_EXPLANATION,
        //SEASON3B::INTERFACE_SETITEM_EXPLANATION,
        INTERFACE_GOLD_BOWMAN,
        INTERFACE_GOLD_BOWMAN_LENA,
        INTERFACE_NPC_DIALOGUE,
        INTERFACE_QUEST_PROGRESS,
        INTERFACE_QUEST_PROGRESS_ETC,
        INTERFACE_EMPIREGUARDIAN_NPC,
#ifdef PBG_MOD_STAMINA_UI
        SEASON3B::INTERFACE_STAMINA_GAUGE,
#endif //PBG_MOD_STAMINA_UI
#ifdef PBG_ADD_GENSRANKING
        INTERFACE_GENSRANKING,
#endif //PBG_ADD_GENSRANKING
        INTERFACE_UNITEDMARKETPLACE_NPC_JULIA,

        SEASON3B::INTERFACE_LUCKYITEMWND,

        0,
    };

    if (m_pNewUIMng)
    {
        for (int i = 0; dwGroupA[i] != 0; i++)
        {
            m_pNewUIMng->ShowInterface(dwGroupA[i], false);
        }
    }
}

void CNewUISystem::HideAllGroupB()
{
    Hide(INTERFACE_FRIEND);
    Hide(INTERFACE_INVENTORY);
    Hide(INTERFACE_CHARACTER);

    DWORD dwGroupB[] = {
        //SEASON3B::INTERFACE_FRIEND,
        //SEASON3B::INTERFACE_INVENTORY,
        //SEASON3B::INTERFACE_CHARACTER,
        //SEASON3B::INTERFACE_WINDOW_MENU,

        INTERFACE_MIXINVENTORY,
        INTERFACE_STORAGE,
        INTERFACE_NPCSHOP,
        INTERFACE_MYSHOP_INVENTORY,
        INTERFACE_PURCHASESHOP_INVENTORY,
        INTERFACE_PET,
        INTERFACE_MYQUEST,
        INTERFACE_NPCQUEST,
        INTERFACE_SENATUS,
        INTERFACE_GUARDSMAN,
        INTERFACE_COMMAND,
        INTERFACE_GUILDINFO,
        INTERFACE_KANTURU2ND_ENTERNPC,
        INTERFACE_CURSEDTEMPLE_NPC,
        INTERFACE_DUELWATCH,
        INTERFACE_DOPPELGANGER_NPC,
        //SEASON3B::INTERFACE_HELP,
        //SEASON3B::INTERFACE_ITEM_EXPLANATION,
        //SEASON3B::INTERFACE_SETITEM_EXPLANATION,
        INTERFACE_GOLD_BOWMAN,
        INTERFACE_GOLD_BOWMAN_LENA,
        INTERFACE_NPC_DIALOGUE,
        INTERFACE_QUEST_PROGRESS,
        INTERFACE_QUEST_PROGRESS_ETC,
        INTERFACE_EMPIREGUARDIAN_NPC,
#ifdef PBG_MOD_STAMINA_UI
        SEASON3B::INTERFACE_STAMINA_GAUGE,
#endif //PBG_MOD_STAMINA_UI
#ifdef PBG_ADD_GENSRANKING
        INTERFACE_GENSRANKING,
#endif //PBG_ADD_GENSRANKING
        INTERFACE_UNITEDMARKETPLACE_NPC_JULIA,
        SEASON3B::INTERFACE_LUCKYITEMWND,

        0,
    };

    if (m_pNewUIMng)
    {
        for (int i = 0; dwGroupB[i] != 0; i++)
        {
            m_pNewUIMng->ShowInterface(dwGroupB[i], false);
        }
    }
}
void CNewUISystem::HideGroupBeforeOpenInterface()
{
    DWORD dwGroupC[] = {
        INTERFACE_COMMAND,
        INTERFACE_GUILDINFO,
        INTERFACE_GOLD_BOWMAN,
        INTERFACE_GOLD_BOWMAN_LENA,
        INTERFACE_GENSRANKING,
        INTERFACE_MUHELPER,
        INTERFACE_MUHELPER_SKILL_LIST,
        0,
    };

    if (m_pNewUIMng)
    {
        for (int i = 0; dwGroupC[i] != 0; i++)
        {
            m_pNewUIMng->ShowInterface(dwGroupC[i], false);
        }
    }
}

void CNewUISystem::UpdateHeroPositionInfoVisibilityForLayoutChange(DWORD dwKey)
{
    if (IsHeroPositionLayoutInterface(dwKey))
    {
        SyncHeroPositionInfoVisibility();
    }
}

void CNewUISystem::SyncHeroPositionInfoVisibility()
{
    if (!m_pNewUIMng)
    {
        return;
    }

    m_pNewUIMng->ShowInterface(INTERFACE_HERO_POSITION_INFO, !ShouldHideHeroPositionInfo());
}

bool CNewUISystem::ShouldHideHeroPositionInfo()
{
    if (!m_pNewUIMng)
    {
        return false;
    }

    if (IsVisible(INTERFACE_STORAGE_EXT))
    {
        return true;
    }

    if (!IsVisible(INTERFACE_INVENTORY_EXT))
    {
        return false;
    }

    return IsVisible(INTERFACE_CHARACTER) || IsVisible(INTERFACE_STORAGE) ||
           IsVisible(INTERFACE_MYSHOP_INVENTORY) || IsVisible(INTERFACE_NPCSHOP) ||
           IsVisible(INTERFACE_MIXINVENTORY) || IsVisible(INTERFACE_TRADE);
}

void CNewUISystem::Enable(DWORD dwKey)
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->EnableInterface(dwKey);
    }
}

void CNewUISystem::Disable(DWORD dwKey)
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->EnableInterface(dwKey, false);
    }
}

bool CNewUISystem::CheckMouseUse()
{
    if (m_pNewUIMng)
    {
        if (m_pNewUIMng->GetActiveMouseUIObj())
            return true;
    }
    return false;
}

bool CNewUISystem::CheckKeyUse()
{
    if (m_pNewUIMng)
    {
        if (m_pNewUIMng->GetActiveKeyUIObj())
            return true;
    }
    return false;
}

bool CNewUISystem::HandleFrameCornerClose(const POINT &winPos, DWORD dwKey)
{
    // Box of the corner glyph in the shared 190-wide frame. Matches the MU Helper
    // close "X" exactly (13x12 anchored at +169,+7) ??the same hit-box the
    // per-window copies used originally, so the click feel is identical across
    // every window. One place to tune for every window that uses this frame.
    constexpr int X_OFFSET = 169, Y_OFFSET = 7, WIDTH = 13, HEIGHT = 12;

    if (IsPress(VK_LBUTTON) &&
        CheckMouseIn(winPos.x + X_OFFSET, winPos.y + Y_OFFSET, WIDTH, HEIGHT))
    {
        Hide(dwKey);
        // Clear the raw button state: world movement reads MouseLButtonPush
        // directly (not the UI consume result), so without this the click falls
        // through and walks the character.
        MouseLButton = false;
        MouseLButtonPop = false;
        MouseLButtonPush = false;
        return true;
    }
    return false;
}

bool CNewUISystem::Update(CTimer2::StartTickTime &timer2StartTickTime)
{

    if (m_pNewUIMng)
    {
        m_pNewUIMng->UpdateMouseEvent();
        m_pNewUIMng->UpdateKeyEvent(*m_pNewUIHotKey);
        return m_pNewUIMng->Update();
    }
    return false;
}

CNewUIManager *CNewUISystem::GetNewUIManager() const
{
    return m_pNewUIMng;
}

CNewUI3DRenderMng *CNewUISystem::GetNewUI3DRenderMng() const
{
    return m_pNewUI3DRenderMng;
}

CNewUIHotKey *CNewUISystem::GetNewUIHotKey() const
{
    return m_pNewUIHotKey;
}

bool CNewUISystem::IsImpossibleSendMoveInterface()
{
    if (IsVisible(INTERFACE_MIXINVENTORY) || IsVisible(INTERFACE_KANTURU2ND_ENTERNPC) ||
        IsVisible(SEASON3B::INTERFACE_LUCKYITEMWND))
    {
        return true;
    }

    return false;
}

bool CNewUISystem::IsImpossibleTradeInterface()
{
    if (IsVisible(INTERFACE_MIXINVENTORY) || IsVisible(INTERFACE_KANTURU2ND_ENTERNPC) ||
        IsVisible(INTERFACE_STORAGE) || IsVisible(INTERFACE_INGAMESHOP)
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
        || IsVisible(INTERFACE_INGAMESHOP)
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME
        || IsVisible(SEASON3B::INTERFACE_LUCKYITEMWND))
    {
        return true;
    }

    return false;
}

bool CNewUISystem::IsImpossibleDuelInterface()
{
    if (IsVisible(INTERFACE_MIXINVENTORY) || IsVisible(INTERFACE_KANTURU2ND_ENTERNPC) ||
        IsVisible(INTERFACE_STORAGE) || IsVisible(INTERFACE_INGAMESHOP)
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
        || IsVisible(INTERFACE_INGAMESHOP)
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME
        || IsVisible(SEASON3B::INTERFACE_LUCKYITEMWND))
    {
        return true;
    }

    return false;
}

bool CNewUISystem::IsImpossibleHideInterface(DWORD dwKey)
{
    if (dwKey == INTERFACE_MAINFRAME || dwKey == INTERFACE_SKILL_LIST ||
        dwKey == INTERFACE_SLIDEWINDOW || dwKey == INTERFACE_MESSAGEBOX ||
        dwKey == INTERFACE_CHATLOGWINDOW || dwKey == INTERFACE_SYSTEMLOGWINDOW ||
        dwKey == INTERFACE_PARTY_INFO_WINDOW || dwKey == INTERFACE_KANTURU_INFO ||
        dwKey == INTERFACE_BLOODCASTLE_TIME || dwKey == INTERFACE_CHAOSCASTLE_TIME ||
        dwKey == INTERFACE_BATTLE_SOCCER_SCORE || dwKey == INTERFACE_DUEL_WINDOW ||
        dwKey == INTERFACE_CRYWOLF || dwKey == INTERFACE_HERO_POSITION_INFO ||
        dwKey == INTERFACE_NAME_WINDOW || dwKey == INTERFACE_SIEGEWARFARE ||
        dwKey == INTERFACE_ITEM_TOOLTIP || dwKey == INTERFACE_HOTKEY ||
        dwKey == INTERFACE_CURSEDTEMPLE_GAMESYSTEM || dwKey == INTERFACE_ITEM_ENDURANCE_INFO ||
        dwKey == INTERFACE_BUFF_WINDOW ||
        (dwKey >= INTERFACE_3DRENDERING_CAMERA_BEGIN &&
         dwKey <= INTERFACE_3DRENDERING_CAMERA_END) ||
        dwKey == INTERFACE_DUELWATCH_MAINFRAME || dwKey == INTERFACE_DUELWATCH_USERLIST ||
        dwKey == INTERFACE_DOPPELGANGER_FRAME || dwKey == INTERFACE_GOLD_BOWMAN ||
        dwKey == INTERFACE_GOLD_BOWMAN_LENA || dwKey == INTERFACE_EMPIREGUARDIAN_TIMER)
    {
        return true;
    }

    return false;
}

void CNewUISystem::UpdateSendMoveInterface()
{
    if (IsVisible(INTERFACE_TRADE))
    {
        SocketClient->ToGameServer()->SendTradeCancel();
        Hide(INTERFACE_TRADE);
    }
    if (IsVisible(INTERFACE_STORAGE_EXT))
    {
        Hide(INTERFACE_STORAGE_EXT);
    }
    if (IsVisible(INTERFACE_STORAGE))
    {
        Hide(INTERFACE_STORAGE);
    }
    if (IsVisible(INTERFACE_NPCGUILDMASTER))
    {
        Hide(INTERFACE_NPCGUILDMASTER);
    }
    if (IsVisible(INTERFACE_MYQUEST))
    {
        Hide(INTERFACE_MYQUEST);
    }
    if (IsVisible(INTERFACE_NPCQUEST))
    {
        Hide(INTERFACE_NPCQUEST);
    }
    if (IsVisible(INTERFACE_NPCSHOP))
    {
        Hide(INTERFACE_NPCSHOP);
    }
    if (IsVisible(INTERFACE_GUARDSMAN))
    {
        Hide(INTERFACE_GUARDSMAN);
    }
    if (IsVisible(INTERFACE_GUARDSMAN))
    {
        Hide(INTERFACE_GUARDSMAN);
    }
    if (IsVisible(INTERFACE_DEVILSQUARE))
    {
        Hide(INTERFACE_DEVILSQUARE);
    }
    if (IsVisible(INTERFACE_BLOODCASTLE))
    {
        Hide(INTERFACE_BLOODCASTLE);
    }
    if (IsVisible(INTERFACE_CURSEDTEMPLE_NPC))
    {
        Hide(INTERFACE_CURSEDTEMPLE_NPC);
    }
    if (IsVisible(INTERFACE_MYSHOP_INVENTORY))
    {
        Hide(INTERFACE_MYSHOP_INVENTORY);
    }
    if (IsVisible(INTERFACE_PURCHASESHOP_INVENTORY))
    {
        Hide(INTERFACE_PURCHASESHOP_INVENTORY);
    }
    if (IsVisible(INTERFACE_DUELWATCH))
    {
        Hide(INTERFACE_DUELWATCH);
    }
    if (IsVisible(INTERFACE_DOPPELGANGER_NPC))
    {
        Hide(INTERFACE_DOPPELGANGER_NPC);
    }
    if (IsVisible(INTERFACE_NPC_DIALOGUE))
    {
        Hide(INTERFACE_NPC_DIALOGUE);
    }
    if (IsVisible(INTERFACE_QUEST_PROGRESS))
    {
        Hide(INTERFACE_QUEST_PROGRESS);
    }
    if (IsVisible(INTERFACE_QUEST_PROGRESS_ETC))
    {
        Hide(INTERFACE_QUEST_PROGRESS_ETC);
    }
    if (IsVisible(INTERFACE_EMPIREGUARDIAN_NPC))
    {
        Hide(INTERFACE_EMPIREGUARDIAN_NPC);
    }
    if (IsVisible(INTERFACE_LUCKYCOIN_REGISTRATION))
    {
        Hide(INTERFACE_LUCKYCOIN_REGISTRATION);
    }
    if (IsVisible(INTERFACE_EXCHANGE_LUCKYCOIN))
    {
        Hide(INTERFACE_EXCHANGE_LUCKYCOIN);
    }
    if (IsVisible(INTERFACE_UNITEDMARKETPLACE_NPC_JULIA))
    {
        Hide(INTERFACE_UNITEDMARKETPLACE_NPC_JULIA);
    }
}

CNewUIChatLogWindow *CNewUISystem::GetUI_NewChatLogWindow() const
{
    return m_pNewChatLogWindow;
}

CNewUISystemLogWindow *CNewUISystem::GetUI_NewSystemLogWindow() const
{
    return m_pNewSystemLogWindow;
}

CNewUISlideWindow *CNewUISystem::GetUI_NewSlideWindow() const
{
    return m_pNewSlideWindow;
}

CNewUIFriendWindow *CNewUISystem::GetUI_NewFriendWindow() const
{
    return m_pNewFriendWindow;
}

CNewUIMainFrameWindow *CNewUISystem::GetUI_NewMainFrameWindow() const
{
    return m_pNewMainFrameWindow;
}

CNewUISkillList *CNewUISystem::GetUI_NewSkillList() const
{
    return m_pNewSkillList;
}

CNewUIChatInputBox *CNewUISystem::GetUI_NewChatInputBox() const
{
    return m_pNewChatInputBox;
}

SessionItemStore *CNewUISystem::GetUI_NewItemMng() const
{
    return m_pNewItemMng;
}

CNewUIMyInventory *CNewUISystem::GetUI_NewMyInventory() const
{
    return m_pNewMyInventory;
}

CNewUIInventoryExtension *CNewUISystem::GetUI_NewMyInventoryExt() const
{
    return m_pNewMyInventoryExt;
}

CNewUINPCShop *CNewUISystem::GetUI_NewNpcShop() const
{
    return m_pNewNPCShop;
}

CNewUIPetInfoWindow *CNewUISystem::GetUI_NewPetInfoWindow() const
{
    return m_pNewPetInfoWindow;
}

CNewUIMixInventory *CNewUISystem::GetUI_NewMixInventory() const
{
    return m_pNewMixInventory;
}

CNewUICastleWindow *CNewUISystem::GetUI_NewCastleWindow() const
{
    return m_pNewCastleWindow;
}

CNewUIGuardWindow *CNewUISystem::GetUI_NewGuardWindow() const
{
    return m_pNewGuardWindow;
}

CNewUIGatemanWindow *CNewUISystem::GetUI_NewGatemanWindow() const
{
    return m_pNewGatemanWindow;
}

CNewUIGateSwitchWindow *CNewUISystem::GetUI_NewGateSwitchWindow() const
{
    return m_pNewGateSwitchWindow;
}

CNewUIStorageInventory *CNewUISystem::GetUI_NewStorageInventory() const
{
    return m_pNewStorageInventory;
}

CNewUIStorageInventoryExt *CNewUISystem::GetUI_NewStorageInventoryExt() const
{
    return m_pNewStorageInventoryExt;
}

CNewUIGuildMakeWindow *CNewUISystem::GetUI_NewGuildMakeWindow() const
{
    return m_pNewGuildMakeWindow;
}

CNewUIGuildInfoWindow *CNewUISystem::GetUI_NewGuildInfoWindow() const
{
    return m_pNewGuildInfoWindow;
}

CNewUICryWolf *CNewUISystem::GetUI_NewCryWolfInterface() const
{
    return m_pNewCryWolfInterface;
}

CNewUIMasterLevel *CNewUISystem::GetUI_NewMasterLevelInterface() const
{
    return m_pNewMaster_Level_Interface;
}

CNewUIMyShopInventory *CNewUISystem::GetUI_NewMyShopInventory() const
{
    return m_pNewMyShopInventory;
}

CNewUIPurchaseShopInventory *CNewUISystem::GetUI_NewPurchaseShopInventory() const
{
    return m_pNewPurchaseShopInventory;
}

CNewUICharacterInfoWindow *CNewUISystem::GetUI_NewCharacterInfoWindow() const
{
    return m_pNewCharacterInfoWindow;
}

CNewUIMyQuestInfoWindow *CNewUISystem::GetUI_NewMyQuestInfoWindow() const
{
    return m_pNewMyQuestInfoWindow;
}

CNewUIPartyListWindow *CNewUISystem::GetUI_NewPartyListWindow() const
{
    return m_pNewPartyListWindow;
}

CNewUINPCQuest *CNewUISystem::GetUI_NewNPCQuest() const
{
    return m_pNewNPCQuest;
}

CNewUIEnterBloodCastle *CNewUISystem::GetUI_NewEnterBloodCastle() const
{
    return m_pNewEnterBloodCastle;
}

CNewUIEnterDevilSquare *CNewUISystem::GetUI_NewEnterDevilSquare() const
{
    return m_pNewEnterDevilSquare;
}

CNewUIBloodCastle *CNewUISystem::GetUI_NewBloodCastle() const
{
    return m_pNewBloodCastle;
}

CNewUITrade *CNewUISystem::GetUI_NewTrade() const
{
    return m_pNewTrade;
}

CNewUIKanturu2ndEnterNpc *CNewUISystem::GetUI_NewKanturu2ndEnterNpc() const
{
    return m_pNewKanturu2ndEnterNpc;
}

CNewUIKanturuInfoWindow *CNewUISystem::GetUI_NewKanturuInfoWindow() const
{
    return m_pNewKanturuInfoWindow;
}

CNewUICatapultWindow *CNewUISystem::GetUI_NewCatapultWindow() const
{
    return m_pNewCatapultWindow;
}

CNewUIChaosCastleTime *CNewUISystem::GetUI_NewChaosCastleTime() const
{
    return m_pNewChaosCastleTime;
}

CNewUICommandWindow *CNewUISystem::GetUI_NewCommandWindow() const
{
    return m_pNewCommandWindow;
}

CNewUIWindowMenu *CNewUISystem::GetUI_NewWindowMenu() const
{
    return m_pNewWindowMenu;
}

CNewUIOptionWindow *CNewUISystem::GetUI_NewOptionWindow() const
{
    return m_pNewOptionWindow;
}

CNewUIHeroPositionInfo *CNewUISystem::GetUI_NewHeroPositionInfo() const
{
    return m_pNewHeroPositionInfo;
}

CNewUIHelpWindow *CNewUISystem::GetUI_NewHelpWindow() const
{
    return m_pNewHelpWindow;
}

CNewUIItemExplanationWindow *CNewUISystem::GetUI_NewItemExplanationWindow() const
{
    return m_pNewItemExplanationWindow;
}

CNewUISetItemExplanation *CNewUISystem::GetUI_NewSetItemExplanation() const
{
    return m_pNewSetItemExplanation;
}

CNewUIQuickCommandWindow *CNewUISystem::GetUI_NewQuickCommandWindow() const
{
    return m_pNewQuickCommandWindow;
}

CNewUIMoveCommandWindow *CNewUISystem::GetUI_NewMoveCommandWindow() const
{
    return m_pNewMoveCommandWindow;
}

CNewUIBattleSoccerScore *CNewUISystem::GetUI_NewBattleSoccerScore() const
{
    return m_pNewBattleSoccerScore;
}

CNewUIDuelWindow *CNewUISystem::GetUI_NewDuelWindow() const
{
    return m_pNewDuelWindow;
}

CNewUISiegeWarfare *CNewUISystem::GetUI_NewSiegeWarfare() const
{
    return m_pNewSiegeWarfare;
}

CNewUIItemEnduranceInfo *CNewUISystem::GetUI_NewItemEnduranceInfo() const
{
    return m_pNewItemEnduranceInfo;
}

CNewUIBuffWindow *CNewUISystem::GetUI_NewBuffWindow() const
{
    return m_pNewBuffWindow;
}

CNewUICursedTempleEnter *CNewUISystem::GetUI_NewCursedTempleEnterWindow() const
{
    return m_pNewCursedTempleEnterWindow;
}

CNewUICursedTempleSystem *CNewUISystem::GetUI_NewCursedTempleWindow() const
{
    return m_pNewCursedTempleWindow;
}

CNewUICursedTempleResult *CNewUISystem::GetUI_NewCursedTempleResultWindow() const
{
    return m_pNewCursedTempleResultWindow;
}

CNewUIGoldBowmanWindow *CNewUISystem::GetUI_pNewGoldBowman() const
{
    return m_pNewGoldBowman;
}

CNewUIGoldBowmanLena *CNewUISystem::GetUI_pNewGoldBowmanLena() const
{
    return m_pNewGoldBowmanLena;
}

CNewUIRegistrationLuckyCoin *CNewUISystem::GetUI_pNewLuckyCoinRegistration() const
{
    return m_pNewLuckyCoinRegistration;
}

CNewUIExchangeLuckyCoin *CNewUISystem::GetUI_pNewExchangeLuckyCoin() const
{
    return m_pNewExchangeLuckyCoinWindow;
}

CNewUIMiniMap *CNewUISystem::GetUI_pNewUIMiniMap() const
{
    return m_pNewMiniMap;
}

CNewUIDuelWatchWindow *CNewUISystem::GetUI_pNewDuelWatch() const
{
    return m_pNewDuelWatchWindow;
}

CNewUIDuelWatchMainFrameWindow *CNewUISystem::GetUI_pNewDuelWatchMainFrame() const
{
    return m_pNewDuelWatchMainFrameWindow;
}

CNewUIDuelWatchUserListWindow *CNewUISystem::GetUI_pNewDuelWatchUserList() const
{
    return m_pNewDuelWatchUserListWindow;
}

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
CNewUIInGameShop *CNewUISystem::GetUI_pNewInGameShop() const
{
    return m_pNewInGameShop;
}
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME

CNewUIDoppelGangerWindow *CNewUISystem::GetUI_pNewDoppelGangerWindow() const
{
    return m_pNewDoppelGangerWindow;
}

CNewUIDoppelGangerFrame *CNewUISystem::GetUI_pNewDoppelGangerFrame() const
{
    return m_pNewDoppelGangerFrame;
}

CNewUINPCDialogue *CNewUISystem::GetUI_NewNPCDialogue() const
{
    return m_pNewNPCDialogue;
}

CNewUIQuestProgress *CNewUISystem::GetUI_NewQuestProgress() const
{
    return m_pNewQuestProgress;
}

CNewUIQuestProgressByEtc *CNewUISystem::GetUI_NewQuestProgressByEtc() const
{
    return m_pNewQuestProgressByEtc;
}

CNewUIEmpireGuardianNPC *CNewUISystem::GetUI_pNewEmpireGuardianNPC() const
{
    return m_pNewEmpireGuardianNPC;
}

CNewUIEmpireGuardianTimer *CNewUISystem::GetUI_pNewEmpireGuardianTimer() const
{
    return m_pNewEmpireGuardianTimer;
}

#ifdef PBG_MOD_STAMINA_UI
CNewUIStamina *SEASON3B::CNewUISystem::GetUI_pNewUIStamina() const
{
    return m_pNewUIStamina;
}
#endif //PBG_MOD_STAMINA_UI

#ifdef PBG_ADD_GENSRANKING
CNewUIGensRanking *CNewUISystem::GetUI_NewGensRanking() const
{
    return m_pNewGensRanking;
}
#endif //PBG_ADD_GENSRANKING

CNewUIUnitedMarketPlaceWindow *CNewUISystem::GetUI_pNewUnitedMarketPlaceWindow() const
{
    return m_pNewUnitedMarketPlaceWindow;
}

CNewUILuckyItemWnd *SEASON3B::CNewUISystem::Get_pNewUILuckyItemWnd() const
{
    return m_pNewUILuckyItemWnd;
}

CNewUIMuHelper *CNewUISystem::Get_pNewUIMuHelper() const
{
    return m_pNewUIMuHelper;
}

CNewUIMuHelperSkillList *CNewUISystem::Get_pNewUIMuHelperSkillList() const
{
    return m_pNewUIMuHelperSkillList;
}

DWORD SessionUiUnit::CreateUIID()
{
    return ++g_dwLastUIID;
}

DWORD SessionLegacyCalls::CreateUIID()
{
    return sessionKeeper_.UiForConstruction().CreateUIID();
}
BOOL SessionUiUnit::CheckMouseIn(int iPos_x, int iPos_y, int iWidth, int iHeight,
                                 int CoordType) const
{
    const int mouseX = MouseX;
    const int mouseY = MouseY;
    if (CoordType == COORDINATE_TYPE_LEFT_DOWN)
    {
        if (mouseX >= iPos_x && mouseX < iPos_x + iWidth && mouseY >= iPos_y - iHeight &&
            mouseY < iPos_y)
            return TRUE;
        else
            return FALSE;
    }
    else
    {
        if (mouseX >= iPos_x && mouseX < iPos_x + iWidth && mouseY >= iPos_y &&
            mouseY < iPos_y + iHeight)
            return TRUE;
        else
            return FALSE;
    }
}

BOOL SessionLegacyCalls::CheckMouseIn(int x, int y, int width, int height, int coordinateType) const
{
    return sessionKeeper_.Ui()->CheckMouseIn(x, y, width, height, coordinateType);
}

// Symmetric counterpart to GiveFocus(): drops keyboard focus from the focused
// portable text field without hiding or destroying it. GiveFocus() sets both
// s_pFocusedPortable and g_dwKeyFocusUIID, so release both here (clearing the
// key-focus id only while it still points at this field, to avoid stomping
// another widget), letting the field hand focus back to the game window while
// staying visible.
// and wherever a paragraph exceeds the box width (wrapped at the last space, or
// mid-word when a single word is too long). Each span is [start, end) in buffer
// indices; end excludes the wrapped space or newline.

void CUIMng::CloseMsgWin()
{
    HideWin(m_MsgWin.get());
}
void CUIWindowMgr::RefreshMainWndPalList()
{
    if (m_dwMainWindowUIID != 0)
        GetFriendMainWindow()->RefreshPalList();
}
void CUIWindowMgr::RefreshMainWndLetterList()
{
    if (m_dwMainWindowUIID != 0)
        GetFriendMainWindow()->RefreshLetterList();
}

// Native feature window methods.
#pragma pack(push)
#pragma pack()
void CUIWindowMgr::ApplyModernUiChanges()
{
    if (g_dwTopWindow != 0 && GetWindow(g_dwTopWindow) == nullptr)
        g_dwTopWindow = 0;

    const std::vector<DWORD> windowIds(m_WindowArrangeList.begin(), m_WindowArrangeList.end());
    for (const DWORD id : windowIds)
    {
        if (CUIBaseWindow *window = GetWindow(id))
        {
            if (window->GetState() != UISTATE_HIDE && window->GetState() != UISTATE_READY)
            {
                window->ApplyModernUiChanges();
                window->DoAction(TRUE);
            }
        }
    }
    while (!m_MessageList.empty())
    {
        GetUIMessage();
        HandleMessage();
    }
    if (m_dwMainWindowUIID > 0 && GetFriendMainWindow() != nullptr &&
        GetFriendMainWindow()->GetState() == UISTATE_HIDE)
    {
        CloseMainWnd();
    }
    m_bRenderFrame = FALSE;
}
#pragma pack(pop)
