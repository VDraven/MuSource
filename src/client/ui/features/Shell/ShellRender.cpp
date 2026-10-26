

#include "ui/features/Shell/ShellRender.h"
#include "I18N/All.h"
#include "app/AppWindow.h"
#include "app/ApplicationAudio.h"
#include "app/ApplicationConfigScheduling.h"
#include "app/ApplicationDiagnostics.h"
#include "app/ApplicationLoopFrame.h"
#include "app/ApplicationNetwork.h"
#include "data/GameData.h"
#include "data/Localization.h"
#include "data/ResourceData.h"
#include "data/WorldData.h"
#include "domain/CharacterPresentation.h"
#include "domain/CharacterSystem.h"
#include "domain/EffectsUpdate.h"
#include "domain/ItemsSkills.h"
#include "domain/MapSimulation.h"
#include "domain/MovementAI.h"
#include "domain/WorldSimulation.h"
#include "render/FrameTape.h"
#include "render/ModelResources.h"
#include "render/Terrain.h"
#include "render/Text.h"
#include "render/Textures.h"
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
#include "support/Scenes.h"
#include "ui/features/Dialogs/DialogsLogic.h"
#include "ui/features/Shell/ShellLogic.h"
#include "ui/features/Social/SocialLogic.h"
#include "ui/runtime/UiControls.h"
#include "ui/runtime/UiRuntime.h"
#include "ui/session/UiSessionLogic.h"

namespace UI::Modern::PC::Character
{
class RmlCharacterCreatePanel::Impl final
{
  public:
    explicit Impl(SessionKeeper &keeper)
        : path_(ResolveUiDocument("Data/UI", UiPlatform::Pc, "Character", "character_create.rml")),
          design_(path_, {"Panel-Size", "Panel-Reference", "Effect-Frames", "Effect-Fps",
                          "Preview-Models", "Preview-Camera"}),
          host_(keeper, "character-create-" + std::to_string(keeper.Id().RawValue()), path_)
    {
    }
    ~Impl()
    {
        Release();
    }
    void Release()
    {
        classes_.Unbind();
        ok_.Unbind();
        cancel_.Unbind();
        effect_.Unbind();
        description_.Unbind();
        panel_ = preview_ = charisma_ = selection_ = nullptr;
        name_ = nullptr;
        classButtons_.fill(nullptr);
        classLabels_.fill(nullptr);
        statTitles_.fill(nullptr);
        stats_.fill(nullptr);
        labels_.fill(nullptr);
        revision_.reset();
        nameValue_.clear();
        changes_ = {};
        visible_ = dirty_ = false;
        resetName_ = focusName_ = true;
        bounds_ = {};
        host_.Release();
    }
    bool Bind()
    {
        auto &doc = *host_.Document();
        panel_ = doc.GetElementById("panel");
        preview_ = doc.GetElementById("mcCharacterRender");
        charisma_ = doc.GetElementById("mcCharisma");
        selection_ = doc.GetElementById("mcSelectEffect");
        auto *effect = doc.GetElementById("mcAnimation");
        auto *ok = doc.GetElementById("btnOK");
        auto *cancel = doc.GetElementById("btnCancel");
        name_ = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(
            doc.GetElementById("tiCharacterName"));
        if (!panel_ || !preview_ || !charisma_ || !selection_ || !effect || !ok || !cancel ||
            !name_)
            return false;
        effect_.Bind(*effect);
        ok_.Bind(*ok);
        cancel_.Bind(*cancel);
        name_->SetAttribute("maxlength", MAX_USERNAME_SIZE);
        for (int i = 0; i < MAX_CLASS; ++i)
        {
            const auto id = "btnClass" + std::to_string(i);
            classButtons_[i] = doc.GetElementById(id);
            classLabels_[i] = doc.GetElementById(id + "-label");
            if (!classButtons_[i] || !classLabels_[i])
                return false;
        }
        classes_.Bind(classButtons_);
        constexpr std::array statIds{"mcStrength", "mcDexterity", "mcVitality", "mcEnergy",
                                     "mcCharisma"};
        for (std::size_t i = 0; i < statIds.size(); ++i)
        {
            statTitles_[i] = doc.GetElementById(std::string(statIds[i]) + "-tfTitle-label");
            stats_[i] = doc.GetElementById(std::string(statIds[i]) + "-tfStat-label");
            if (!statTitles_[i] || !stats_[i])
                return false;
        }
        labels_ = {doc.GetElementById("tfNameTitle-label"),
                   doc.GetElementById("tfClassTitle-label"), doc.GetElementById("btnOK-label"),
                   doc.GetElementById("btnCancel-label")};
        return std::all_of(labels_.begin(), labels_.end(), [](auto *p) { return p != nullptr; }) &&
               description_.Bind(doc, "taExplain-label");
    }
    static void Text(Rml::Element *element, const std::wstring &text)
    {
        element->SetInnerRML(
            Rml::StringUtilities::EncodeRml(StringUtils::WideToNarrow(text.c_str())));
    }
    bool Apply(const Content &content)
    {
        if (revision_ == content.revision)
            return false;
        for (int i = 0; i < MAX_CLASS; ++i)
            Text(classLabels_[i], content.classes[i]);
        for (std::size_t i = 0; i < stats_.size(); ++i)
        {
            Text(statTitles_[i], content.statTitles[i]);
            Text(stats_[i], content.stats[i]);
        }
        Text(labels_[0], content.nameTitle);
        Text(labels_[1], content.classTitle);
        Text(labels_[2], content.ok);
        Text(labels_[3], content.cancel);
        description_.SetMarkup(Rml::StringUtilities::EncodeRml(
            StringUtils::WideToNarrow(content.description.c_str())));
        charisma_->SetProperty("display", content.showCharisma ? "block" : "none");
        auto *button = classButtons_[content.selected];
        const float center =
            button->GetProperty<float>("top") + std::ceil(button->GetProperty<float>("height") / 2);
        selection_->SetProperty(Rml::PropertyId::Top, Rml::Property(center, Rml::Unit::PX));
        ok_.SetEnable(content.enabled[content.selected]);
        revision_ = content.revision;
        return true;
    }
    bool Prepare(int width, int height, bool visible, double elapsed, const Content &content)
    {
        if (!panel_ && !visible)
            return true;
        const auto size = design_.Values(0);
        if (!host_.Ensure(width, height, size[0], size[1]))
            return false;
        if (!panel_ && !Bind())
        {
            Release();
            return false;
        }
        if (!host_.SetVisible(visible))
            return false;
        visible_ = visible;
        if (resetName_)
        {
            name_->SetValue("");
            resetName_ = false;
            dirty_ = true;
        }
        if (visible && focusName_ && name_->Focus(true))
        {
            focusName_ = false;
            dirty_ = true;
        }
        if (!visible)
        {
            changes_ = {};
            ok_.Reset();
            cancel_.Reset();
        }
        dirty_ = Apply(content) || dirty_;
        dirty_ = classes_.Apply(content.selected, content.enabled) || dirty_;
        dirty_ = ok_.SyncVisualState() || dirty_;
        dirty_ = cancel_.SyncVisualState() || dirty_;
        constexpr double MillisecondsPerSecond = 1000;
        const int frame =
            std::min(design_.Number<int>(2),
                     1 + static_cast<int>(elapsed * design_.Number(3) / MillisecondsPerSecond));
        dirty_ = effect_.SetFrame(frame) || dirty_;
        dirty_ = description_.Apply() || dirty_;
        if (!host_.CaptureIfDirty(dirty_))
            return false;
        dirty_ = false;
        const auto viewport = host_.Viewport();
        const auto reference = design_.Values(1);
        const auto offset = preview_->GetAbsoluteOffset();
        const auto extent = preview_->GetBox().GetSize();
        bounds_ = {
            offset.x * reference[0] / viewport.width, offset.y * reference[1] / viewport.height,
            extent.x * reference[0] / viewport.width, extent.y * reference[1] / viewport.height};
        return true;
    }
    bool Input(const SessionInputEvent &event)
    {
        if (!visible_)
            return false;
        host_.ProcessInput(event);
        if (auto selected = classes_.TakeSelection())
            changes_.selected = selected;
        changes_.ok = ok_.IsClick() || changes_.ok;
        changes_.cancel = cancel_.IsClick() || changes_.cancel;
        nameValue_ = StringUtils::NarrowToWide(name_->GetValue().c_str());
        dirty_ = true;
        if (event.kind == SessionInputEventKind::Key &&
            (event.code == SDL_SCANCODE_RETURN || event.code == SDL_SCANCODE_KP_ENTER ||
             event.code == SDL_SCANCODE_ESCAPE))
            return false;
        return event.kind == SessionInputEventKind::Pointer ||
               event.kind == SessionInputEventKind::Text ||
               event.kind == SessionInputEventKind::Key;
    }
    std::filesystem::path path_;
    RmlUiDesign design_;
    RmlDocumentHost host_;
    RmlMuButtonGroup classes_;
    RmlMuButton ok_, cancel_;
    RmlMuProgressBar effect_;
    RmlMuTextArea description_;
    Rml::Element *panel_ = nullptr, *preview_ = nullptr, *charisma_ = nullptr,
                 *selection_ = nullptr;
    Rml::ElementFormControlInput *name_ = nullptr;
    std::array<Rml::Element *, MAX_CLASS> classButtons_{}, classLabels_{};
    std::array<Rml::Element *, 5> statTitles_{}, stats_{};
    std::array<Rml::Element *, 4> labels_{};
    std::optional<std::uint64_t> revision_;
    std::wstring nameValue_;
    Changes changes_;
    Rect bounds_;
    bool visible_ = false, dirty_ = false, resetName_ = true, focusName_ = true;
};
RmlCharacterCreatePanel::RmlCharacterCreatePanel(SessionKeeper &keeper)
    : impl_(std::make_unique<Impl>(keeper))
{
}
RmlCharacterCreatePanel::~RmlCharacterCreatePanel() = default;
void RmlCharacterCreatePanel::Release()
{
    impl_->Release();
}
void RmlCharacterCreatePanel::ClearName()
{
    impl_->nameValue_.clear();
    impl_->resetName_ = impl_->focusName_ = true;
}
const std::wstring &RmlCharacterCreatePanel::Name() const
{
    return impl_->nameValue_;
}
std::optional<RmlTextInputArea> RmlCharacterCreatePanel::TextInputArea() const
{
    return impl_->visible_ ? impl_->host_.FocusedTextInputArea() : std::nullopt;
}
RmlCharacterCreatePanel::Rect RmlCharacterCreatePanel::PreviewBounds() const
{
    return impl_->bounds_;
}
std::span<const float> RmlCharacterCreatePanel::ModelParameters(int classIndex) const
{
    constexpr int ParametersPerClass = 7;
    return impl_->design_.Values(4).subspan(classIndex * ParametersPerClass, ParametersPerClass);
}
std::span<const float> RmlCharacterCreatePanel::PreviewCamera() const
{
    return impl_->design_.Values(5);
}
bool RmlCharacterCreatePanel::PrepareOnWorker(int width, int height, bool visible, double elapsed,
                                              const Content &content)
{
    return impl_->Prepare(width, height, visible, elapsed, content);
}
bool RmlCharacterCreatePanel::ProcessInput(const SessionInputEvent &event)
{
    return impl_->Input(event);
}
RmlCharacterCreatePanel::Changes RmlCharacterCreatePanel::TakeChanges()
{
    return std::exchange(impl_->changes_, {});
}
bool RmlCharacterCreatePanel::Record(LegacyRenderFacade &facade) const
{
    return !impl_->panel_ || impl_->host_.Record(facade);
}
} // namespace UI::Modern::PC::Character

namespace UI::Modern::PC::Login
{
namespace
{
enum class LoginPanelDesignKey
{
    Width,
    Height,
    AnchorX,
    AnchorY
};

const RmlUiDesign &LoginPanelDesign()
{
    static const RmlUiDesign design(
        "Data/UI/PC/Login/login.rml",
        {"Login-Width", "Login-Height", "Login-AnchorX", "Login-AnchorY"});
    return design;
}

enum class PendingFocus
{
    None,
    Account,
    Password,
};

bool SetText(Rml::Element &element, std::wstring &current, const std::wstring &next)
{
    if (current == next)
    {
        return false;
    }
    current = next;
    element.SetInnerRML(Rml::StringUtilities::EncodeRml(StringUtils::WideToNarrow(next.c_str())));
    return true;
}
} // namespace

float RmlLoginPanel::Width() noexcept
{
    return LoginPanelDesign().Number(LoginPanelDesignKey::Width);
}
float RmlLoginPanel::Height() noexcept
{
    return LoginPanelDesign().Number(LoginPanelDesignKey::Height);
}

int RmlLoginPanel::LeftFor(int viewportWidth) noexcept
{
    return static_cast<int>((viewportWidth - Width()) *
                            LoginPanelDesign().Number(LoginPanelDesignKey::AnchorX));
}
int RmlLoginPanel::TopFor(int viewportHeight) noexcept
{
    return static_cast<int>((viewportHeight - Height()) *
                            LoginPanelDesign().Number(LoginPanelDesignKey::AnchorY));
}

class RmlLoginPanel::Impl final
{
  public:
    Impl(SessionKeeper &keeper, RmlMuButton &ok, RmlMuButton &cancel)
        : okButton_(ok), cancelButton_(cancel),
          host_(keeper, "login-" + std::to_string(keeper.Id().RawValue()),
                ResolveUiDocument("Data/UI", UiPlatform::Pc, "Login", "login.rml"))
    {
    }

    ~Impl()
    {
        Release();
    }

    void Release()
    {
        okButton_.Unbind();
        cancelButton_.Unbind();
        panel_ = nullptr;
        serverName_ = nullptr;
        accountLabel_ = nullptr;
        passwordLabel_ = nullptr;
        accountInput_ = nullptr;
        passwordInput_ = nullptr;
        ok_ = nullptr;
        cancel_ = nullptr;
        okLabel_ = nullptr;
        cancelLabel_ = nullptr;
        currentContent_ = {};
        account_.clear();
        password_.clear();
        x_ = 0;
        y_ = 0;
        visible_ = false;
        positionDirty_ = true;
        credentialsDirty_ = true;
        pendingFocus_ = PendingFocus::None;
        host_.Release();
    }

    void SetPosition(int x, int y) noexcept
    {
        if (x_ == x && y_ == y)
        {
            return;
        }
        x_ = x;
        y_ = y;
        positionDirty_ = true;
    }

    void Show(bool show) noexcept
    {
        if (!show)
        {
            pendingFocus_ = PendingFocus::None;
        }
        visible_ = show;
    }

    void SetCredentials(const std::wstring &account, const std::wstring &password)
    {
        if (account_ == account && password_ == password)
        {
            return;
        }
        account_ = account;
        password_ = password;
        credentialsDirty_ = true;
    }

    const std::wstring &Account() const noexcept
    {
        return account_;
    }
    const std::wstring &Password() const noexcept
    {
        return password_;
    }

    void FocusInitialInput() noexcept
    {
        pendingFocus_ = account_.empty() ? PendingFocus::Account : PendingFocus::Password;
    }

    void FocusAccountInput() noexcept
    {
        pendingFocus_ = PendingFocus::Account;
    }

    void FocusPasswordInput() noexcept
    {
        pendingFocus_ = PendingFocus::Password;
    }

    bool ProcessInput(const SessionInputEvent &event)
    {
        if (!visible_ || !host_.ProcessInput(event))
        {
            return false;
        }
        SyncInputValues();
        return true;
    }

    std::optional<RmlTextInputArea> TextInputArea() const
    {
        return visible_ ? host_.FocusedTextInputArea() : std::nullopt;
    }

    bool Prepare(int viewportWidth, int viewportHeight, const RmlLoginContent &content)
    {
        if (panel_ == nullptr && !visible_)
            return true;
        bool dirty = false;
        if (!EnsureDocument(viewportWidth, viewportHeight))
        {
            return false;
        }
        if (!host_.SetVisible(visible_))
        {
            return false;
        }
        dirty = ApplyPendingInputState() || dirty;
        dirty = ApplyPosition() || dirty;

        dirty = SetText(*serverName_, currentContent_.serverName, content.serverName) || dirty;
        dirty =
            SetText(*accountLabel_, currentContent_.accountLabel, content.accountLabel) || dirty;
        dirty =
            SetText(*passwordLabel_, currentContent_.passwordLabel, content.passwordLabel) || dirty;
        dirty = SetText(*okLabel_, currentContent_.okLabel, content.okLabel) || dirty;
        dirty = SetText(*cancelLabel_, currentContent_.cancelLabel, content.cancelLabel) || dirty;
        dirty = okButton_.SyncVisualState() || dirty;
        dirty = cancelButton_.SyncVisualState() || dirty;
        return host_.CaptureIfDirty(dirty);
    }

    bool Record(LegacyRenderFacade &facade) const
    {
        return host_.Record(facade);
    }

  private:
    bool ApplyPosition()
    {
        const RmlUiScaledViewport viewport = host_.Viewport();
        SetPosition(LeftFor(viewport.width), TopFor(viewport.height));
        if (positionDirty_)
        {
            panel_->SetProperty("left", Rml::CreateString("%dpx", x_));
            panel_->SetProperty("top", Rml::CreateString("%dpx", y_));
            positionDirty_ = false;
            return true;
        }

        return false;
    }

    bool EnsureDocument(int viewportWidth, int viewportHeight)
    {
        if (!host_.Ensure(viewportWidth, viewportHeight, Width(), Height()))
        {
            return false;
        }
        if (panel_ != nullptr)
        {
            return true;
        }
        Rml::ElementDocument *const document = host_.Document();
        panel_ = document->GetElementById("login-panel");
        serverName_ = document->GetElementById("server-name");
        accountLabel_ = document->GetElementById("account-label");
        passwordLabel_ = document->GetElementById("password-label");
        accountInput_ = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(
            document->GetElementById("account-input"));
        passwordInput_ = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(
            document->GetElementById("password-input"));
        ok_ = document->GetElementById("ok-button");
        cancel_ = document->GetElementById("cancel-button");
        okLabel_ = document->GetElementById("ok-label");
        cancelLabel_ = document->GetElementById("cancel-label");
        if (panel_ == nullptr || serverName_ == nullptr || accountLabel_ == nullptr ||
            passwordLabel_ == nullptr || accountInput_ == nullptr || passwordInput_ == nullptr ||
            ok_ == nullptr || cancel_ == nullptr || okLabel_ == nullptr || cancelLabel_ == nullptr)
        {
            Release();
            return false;
        }
        okButton_.Bind(*ok_);
        cancelButton_.Bind(*cancel_);
        accountInput_->SetAttribute("maxlength", MAX_USERNAME_SIZE);
        passwordInput_->SetAttribute("maxlength", MAX_PASSWORD_SIZE);
        positionDirty_ = true;
        return true;
    }

    bool ApplyPendingInputState()
    {
        bool dirty = false;
        if (credentialsDirty_)
        {
            accountInput_->SetValue(StringUtils::WideToNarrow(account_.c_str()));
            passwordInput_->SetValue(StringUtils::WideToNarrow(password_.c_str()));
            credentialsDirty_ = false;
            dirty = true;
        }
        if (visible_ && pendingFocus_ != PendingFocus::None)
        {
            Rml::ElementFormControlInput *const input =
                pendingFocus_ == PendingFocus::Account ? accountInput_ : passwordInput_;
            if (input->Focus(true))
            {
                pendingFocus_ = PendingFocus::None;
                dirty = true;
            }
        }
        return dirty;
    }

    void SyncInputValues()
    {
        if (accountInput_ == nullptr || passwordInput_ == nullptr)
        {
            return;
        }
        account_ = StringUtils::NarrowToWide(accountInput_->GetValue().c_str());
        password_ = StringUtils::NarrowToWide(passwordInput_->GetValue().c_str());
    }

    RmlMuButton &okButton_;
    RmlMuButton &cancelButton_;
    RmlDocumentHost host_;
    Rml::Element *panel_ = nullptr;
    Rml::Element *serverName_ = nullptr;
    Rml::Element *accountLabel_ = nullptr;
    Rml::Element *passwordLabel_ = nullptr;
    Rml::ElementFormControlInput *accountInput_ = nullptr;
    Rml::ElementFormControlInput *passwordInput_ = nullptr;
    Rml::Element *ok_ = nullptr;
    Rml::Element *cancel_ = nullptr;
    Rml::Element *okLabel_ = nullptr;
    Rml::Element *cancelLabel_ = nullptr;
    RmlLoginContent currentContent_;
    std::wstring account_;
    std::wstring password_;
    int x_ = 0;
    int y_ = 0;
    bool visible_ = false;
    bool positionDirty_ = true;
    bool credentialsDirty_ = true;
    PendingFocus pendingFocus_ = PendingFocus::None;
};

RmlLoginPanel::RmlLoginPanel(SessionKeeper &keeper)
    : buttons_(keeper), impl_(std::make_unique<Impl>(keeper, buttons_[0], buttons_[1]))
{
}

RmlLoginPanel::~RmlLoginPanel() = default;

void RmlLoginPanel::Create()
{
    for (RmlMuButton &button : buttons_)
    {
        button.SetEnable(true);
        button.Reset();
    }
}

void RmlLoginPanel::Release()
{
    impl_->Release();
}

void RmlLoginPanel::SetPosition(int x, int y)
{
    impl_->SetPosition(x, y);
}

void RmlLoginPanel::Show(bool show)
{
    if (!show)
    {
        for (RmlMuButton &button : buttons_)
        {
            button.Reset();
        }
    }
    impl_->Show(show);
}

void RmlLoginPanel::SetCredentials(const std::wstring &account, const std::wstring &password)
{
    impl_->SetCredentials(account, password);
}

const std::wstring &RmlLoginPanel::Account() const noexcept
{
    return impl_->Account();
}

const std::wstring &RmlLoginPanel::Password() const noexcept
{
    return impl_->Password();
}

void RmlLoginPanel::FocusInitialInput()
{
    impl_->FocusInitialInput();
}

void RmlLoginPanel::FocusAccountInput()
{
    impl_->FocusAccountInput();
}

void RmlLoginPanel::FocusPasswordInput()
{
    impl_->FocusPasswordInput();
}

bool RmlLoginPanel::ProcessInput(const SessionInputEvent &event)
{
    return impl_->ProcessInput(event);
}

std::optional<RmlTextInputArea> RmlLoginPanel::TextInputArea() const
{
    return impl_->TextInputArea();
}

bool RmlLoginPanel::PrepareOnWorker(int viewportWidth, int viewportHeight,
                                    const RmlLoginContent &content)
{
    return impl_->Prepare(viewportWidth, viewportHeight, content);
}

bool RmlLoginPanel::Record(LegacyRenderFacade &facade)
{
    return impl_->Record(facade);
}
} // namespace UI::Modern::PC::Login

namespace UI::Modern::PC::Login
{
namespace
{
enum class LoginSceneButtonsDesignKey
{
    Width,
    Height,
    HorizontalMargin,
    BottomMargin,
    BaselineY,
    Columns
};
const RmlUiDesign &LoginSceneButtonsDesign()
{
    static const RmlUiDesign design("Data/UI/PC/Login/login_bottom.rml",
                                    {"LoginBottom-Width", "LoginBottom-Height",
                                     "LoginBottom-HorizontalMargin", "LoginBottom-BottomMargin",
                                     "LoginBottom-BaselineY", "LoginBottom-Columns"});
    return design;
}
} // namespace

int RmlLoginSceneButtons::ButtonWidth() noexcept
{
    return LoginSceneButtonsDesign().Number<int>(LoginSceneButtonsDesignKey::Width);
}
int RmlLoginSceneButtons::ButtonHeight() noexcept
{
    return LoginSceneButtonsDesign().Number<int>(LoginSceneButtonsDesignKey::Height);
}
int RmlLoginSceneButtons::HorizontalMargin() noexcept
{
    return LoginSceneButtonsDesign().Number<int>(LoginSceneButtonsDesignKey::HorizontalMargin);
}
int RmlLoginSceneButtons::ButtonY(int viewportHeight) noexcept
{
    return static_cast<int>(
               LoginSceneButtonsDesign().Number(LoginSceneButtonsDesignKey::BaselineY) *
               viewportHeight) -
           ButtonHeight() -
           LoginSceneButtonsDesign().Number<int>(LoginSceneButtonsDesignKey::BottomMargin);
}

class RmlLoginSceneButtons::Impl final
{
  public:
    Impl(SessionKeeper &keeper, RmlMuButton &menu, RmlMuButton &credit)
        : menuButton_(menu), creditButton_(credit),
          host_(keeper, "login-buttons-" + std::to_string(keeper.Id().RawValue()),
                ResolveUiDocument("Data/UI", UiPlatform::Pc, "Login", "login_bottom.rml"))
    {
    }

    ~Impl()
    {
        Release();
    }

    void Release()
    {
        menuButton_.Unbind();
        creditButton_.Unbind();
        menu_ = nullptr;
        credit_ = nullptr;
        menuX_ = 0;
        creditX_ = 0;
        y_ = 0;
        visible_ = false;
        positionDirty_ = true;
        pointerOwned_ = false;
        host_.Release();
    }

    void SetPosition(int menuX, int creditX, int y) noexcept
    {
        if (menuX_ == menuX && creditX_ == creditX && y_ == y)
        {
            return;
        }
        menuX_ = menuX;
        creditX_ = creditX;
        y_ = y;
        positionDirty_ = true;
    }

    void Show(bool show) noexcept
    {
        if (!show)
        {
            pointerOwned_ = false;
        }
        visible_ = show;
    }

    bool ProcessInput(const SessionInputEvent &event)
    {
        if (!visible_ || !host_.ProcessInput(event))
        {
            return false;
        }
        const Rml::Element *const pointerTarget = host_.HoverElement();
        const bool pointerOverButton =
            menuButton_.OwnsPointer(pointerTarget) || creditButton_.OwnsPointer(pointerTarget);
        switch (event.action)
        {
        case SessionInputAction::PointerMove:
        case SessionInputAction::PointerWheel:
            return pointerOwned_ || pointerOverButton;
        case SessionInputAction::PointerButton:
            if (event.pressed)
            {
                pointerOwned_ = pointerOverButton;
                return pointerOwned_;
            }
            {
                const bool owned = pointerOwned_ || pointerOverButton;
                pointerOwned_ = false;
                return owned;
            }
        case SessionInputAction::WindowFocusLost:
            pointerOwned_ = false;
            return false;
        default:
            return false;
        }
    }

    bool Prepare(int viewportWidth, int viewportHeight)
    {
        if (menu_ == nullptr && !visible_)
            return true;
        bool dirty = false;
        if (!EnsureDocument(viewportWidth, viewportHeight))
        {
            return false;
        }
        if (!host_.SetVisible(visible_))
        {
            return false;
        }
        dirty = ApplyPosition() || dirty;
        dirty = menuButton_.SyncVisualState() || dirty;
        dirty = creditButton_.SyncVisualState() || dirty;
        return host_.CaptureIfDirty(dirty);
    }

    bool Record(LegacyRenderFacade &facade) const
    {
        return host_.Record(facade);
    }

  private:
    bool ApplyPosition()
    {
        const RmlUiScaledViewport viewport = host_.Viewport();
        SetPosition(HorizontalMargin(), viewport.width - HorizontalMargin() - ButtonWidth(),
                    ButtonY(viewport.height));
        if (positionDirty_)
        {
            menu_->SetProperty("left", Rml::CreateString("%dpx", menuX_));
            menu_->SetProperty("top", Rml::CreateString("%dpx", y_));
            credit_->SetProperty("left", Rml::CreateString("%dpx", creditX_));
            credit_->SetProperty("top", Rml::CreateString("%dpx", y_));
            positionDirty_ = false;
            return true;
        }
        return false;
    }

    bool EnsureDocument(int viewportWidth, int viewportHeight)
    {
        if (!host_.Ensure(
                viewportWidth, viewportHeight,
                (ButtonWidth() + HorizontalMargin()) *
                    LoginSceneButtonsDesign().Number<int>(LoginSceneButtonsDesignKey::Columns),
                ButtonHeight() +
                    LoginSceneButtonsDesign().Number(LoginSceneButtonsDesignKey::BottomMargin)))
        {
            return false;
        }
        if (menu_ != nullptr && credit_ != nullptr)
        {
            return true;
        }
        Rml::ElementDocument *const document = host_.Document();
        menu_ = document->GetElementById("menu-button");
        credit_ = document->GetElementById("credit-button");
        if (menu_ == nullptr || credit_ == nullptr)
        {
            Release();
            return false;
        }
        menuButton_.Bind(*menu_);
        creditButton_.Bind(*credit_);
        positionDirty_ = true;
        return true;
    }

    RmlMuButton &menuButton_;
    RmlMuButton &creditButton_;
    RmlDocumentHost host_;
    Rml::Element *menu_ = nullptr;
    Rml::Element *credit_ = nullptr;
    int menuX_ = 0;
    int creditX_ = 0;
    int y_ = 0;
    bool visible_ = false;
    bool positionDirty_ = true;
    bool pointerOwned_ = false;
};

RmlLoginSceneButtons::RmlLoginSceneButtons(SessionKeeper &keeper)
    : buttons_(keeper), impl_(std::make_unique<Impl>(keeper, buttons_[0], buttons_[1]))
{
}

RmlLoginSceneButtons::~RmlLoginSceneButtons() = default;

void RmlLoginSceneButtons::Create()
{
    for (RmlMuButton &button : buttons_)
    {
        button.SetEnable(true);
        button.Reset();
    }
    CreditButton().SetEnable(false);
}

void RmlLoginSceneButtons::Release()
{
    impl_->Release();
}

void RmlLoginSceneButtons::SetPosition(int menuX, int creditX, int y)
{
    impl_->SetPosition(menuX, creditX, y);
}

void RmlLoginSceneButtons::Show(bool show)
{
    if (!show)
    {
        for (RmlMuButton &button : buttons_)
        {
            button.Reset();
        }
    }
    impl_->Show(show);
}

bool RmlLoginSceneButtons::ProcessInput(const SessionInputEvent &event)
{
    return impl_->ProcessInput(event);
}

bool RmlLoginSceneButtons::PrepareOnWorker(int viewportWidth, int viewportHeight)
{
    return impl_->Prepare(viewportWidth, viewportHeight);
}

bool RmlLoginSceneButtons::Record(LegacyRenderFacade &facade)
{
    return impl_->Record(facade);
}
} // namespace UI::Modern::PC::Login

//*****************************************************************************
//*****************************************************************************

//=============================================================================
// Global Variables
//=============================================================================

//=============================================================================
// Constructor / Destructor
//=============================================================================

//=============================================================================
// Public Methods
//=============================================================================

void CLoginMainWin::SetPosition(int nXCoord, int nYCoord)
{
    CWin::SetPosition(nXCoord, nYCoord);

    const int creditX =
        nXCoord + CWin::GetWidth() - UI::Modern::PC::Login::RmlLoginSceneButtons::ButtonWidth();
    m_sceneButtons.SetPosition(nXCoord, creditX, nYCoord);
}

void CLoginMainWin::Show(bool bShow)
{
    CWin::Show(bShow);

    m_sceneButtons.Show(bShow);
}

void CLoginMainWin::RenderControls()
{
    (void)m_sceneButtons.Record(LegacyRender());
}

bool CLoginMainWin::PrepareModernUiOnWorker(int viewportWidth, int viewportHeight)
{
    return m_sceneButtons.PrepareOnWorker(viewportWidth, viewportHeight);
}

//*****************************************************************************
// Desc: implementation of the CServerSelWin class.
//*****************************************************************************

#define SSW_GAP_WIDTH 28
#define SSW_GAP_HEIGHT 5
#define SSW_GB_POS_X 16
#define SSW_GB_POS_Y 19

using namespace SEASON3A;

void CServerSelWin::SetPosition(int nXCoord, int nYCoord)
{
    CWin::SetPosition(nXCoord, nYCoord);

    int nServerGBtnWidth = m_aServerGroupBtn[0].GetWidth();
    int nServerGBtnHeight = m_aServerGroupBtn[0].GetHeight();
    int nServerBtnWidth = m_aServerBtn[0].GetWidth();
    int nServerBtnHeight = m_aServerBtn[0].GetHeight();
    int nDescGgHeight = m_winDescription.GetHeight();
    int nBtnPosY;
    int i;

    int nServerGBtnBasePosY =
        nYCoord + CWin::GetHeight() - (nServerGBtnHeight * 11 + SSW_GAP_HEIGHT * 2 + nDescGgHeight);
    int nRServerGBtnPosX = nXCoord + nServerGBtnWidth + nServerBtnWidth + (SSW_GAP_WIDTH * 2);

    int icntServreGroup = 0;
    m_aServerGroupBtn[icntServreGroup++].SetPosition(
        nXCoord + (CWin::GetWidth() - nServerGBtnWidth) / 2,
        nYCoord + CWin::GetHeight() - nServerGBtnHeight - SSW_GAP_HEIGHT - nDescGgHeight);

    for (i = 0; i < SSW_LEFT_SERVER_G_MAX; i++)
    {
        nBtnPosY = nServerGBtnBasePosY + nServerGBtnHeight * i;
        m_aServerGroupBtn[icntServreGroup++].SetPosition(nXCoord, nBtnPosY);
    }

    for (i = 0; i < SSW_RIGHT_SERVER_G_MAX; i++)
    {
        nBtnPosY = nServerGBtnBasePosY + nServerGBtnHeight * i;
        m_aServerGroupBtn[icntServreGroup++].SetPosition(nRServerGBtnPosX, nBtnPosY);
    }

    m_winDescription.SetPosition(nXCoord - ((m_winDescription.GetWidth() - CWin::GetWidth()) / 2),
                                 nYCoord + CWin::GetHeight() - m_winDescription.GetHeight());

    m_aBtnDeco[0].SetPosition(m_aServerGroupBtn[1].GetXPos(), m_aServerGroupBtn[1].GetYPos());
    m_aBtnDeco[1].SetPosition(m_aServerGroupBtn[SSW_LEFT_SERVER_G_MAX + 1].GetXPos() +
                                  SERVER_GROUP_BTN_WIDTH,
                              m_aServerGroupBtn[SSW_LEFT_SERVER_G_MAX + 1].GetYPos());

    int a = m_aServerGroupBtn[1].GetXPos();
}

void CServerSelWin::Show(bool bShow)
{
    CWin::Show(bShow);
}

void CServerSelWin::RenderControls()
{
    int i = 0;

    g_RenderText.SetFont(LegacyFontRole::Fixed);
    g_RenderText.SetTextColor(CLRDW_WHITE);
    g_RenderText.SetBgColor(0);

    CWin::RenderButtons();

    if (m_pSelectServerGroup != NULL)
    {
        for (i = 0; i < m_icntServer; i++)
        {
            m_aServerGauge[i].Render();
        }

        if (m_pSelectServerGroup->m_bPvPServer == true)
        {
            g_RenderText.SetTextColor(ARGB(255, 255, 255, 255));
            g_RenderText.RenderText(90, 164 - 60, I18N::Game::SinceHelheimServer);
            g_RenderText.RenderText(90, 164 - 45, I18N::Game::TendsToBeCrowded);
            g_RenderText.RenderText(90, 164 - 30, I18N::Game::WeRecommendThatYouUseOtherServers);
        }
    }
}

void CLoginWin::BindConfiguration(SessionSlotId slotId, SessionConfigValues &values,
                                  SessionConfigStore &store) noexcept
{
    (void)slotId;
    m_sessionConfig = &values;
    m_sessionConfigStore = &store;
}

void CLoginWin::SetPosition(int x, int y)
{
    CWin::SetPosition(x, y);

    m_panel.SetPosition(x, y);
}

void CLoginWin::Show(bool bShow)
{
    CWin::Show(bShow);

    m_panel.Show(bShow);
}

bool CLoginWin::PrepareModernUiOnWorker(int viewportWidth, int viewportHeight)
{
    wchar_t serverName[MAX_TEXT_LENGTH] = {};
    const wchar_t *serverStatus =
        g_ServerListManager.GetNonPVPInfo() ? I18N::Game::SDServer : I18N::Game::SDNonPvPServer;
    mu_swprintf(serverName, serverStatus, g_ServerListManager.GetSelectServerName(),
                g_ServerListManager.GetSelectServerIndex());

    const UI::Modern::PC::Login::RmlLoginContent content{
        serverName, I18N::Game::Account, I18N::Game::Password, I18N::Game::OK, I18N::Game::Cancel};
    return m_panel.PrepareOnWorker(viewportWidth, viewportHeight, content);
}

void CLoginWin::RenderControls()
{
    (void)m_panel.Record(LegacyRender());
}

///////////////////////////////////////////////////////////////////////////////
// CharacterScene.cpp - Character selection scene implementation
///////////////////////////////////////////////////////////////////////////////

// Forward declaration
BOOL Util_CheckOption(std::wstring lpszCommandLine, wchar_t cOption, std::wstring &lpszString);

/**
 * @brief Renders UI elements for character selection scene.
 */
void SessionRenderUnit::RenderCharacterSceneUI()
{
    if (!SwitchWorldRenderTapePass(RenderTapePass::Sprites, 0, 0, REFERENCE_WIDTH,
                                   REFERENCE_HEIGHT))
    {
        return;
    }
    BeginSprite();
    RenderSprites();
    RenderParticles();
    RenderPoints();
    EndSprite();
    EndOpengl();

    if (!BeginRenderTapePass(RenderTapePass::UserInterface))
    {
        return;
    }
    BeginBitmap();

    RenderInfomation();

#ifdef ENABLE_EDIT
    RenderDebugWindow();
#endif

    // The character scene skips the full NewUI render.
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION))
    {
        g_pOption->UpdateMouseEvent();
        g_pOption->UpdateKeyEvent();
        g_pOption->Update();
        g_pOption->Render();
    }

    RenderCursor();

    EndBitmap();
}

// OMF-01767
void SessionLegacyCalls::RenderCharacterSceneUI()
{
    return sessionKeeper_.Renderer()->RenderCharacterSceneUI();
} // OMF-01769
// OMF-01770

//*****************************************************************************
//*****************************************************************************

void CCharMakeWin::RenderControls()
{
    RenderCreateCharacter();
    panel_.Record(sessionKeeper_.Renderer()->LegacyRender());
}

void CCharMakeWin::PrepareCreateCharacterAppearance()
{
    auto &object = CharacterView.Object;
    const auto parameters = panel_.ModelParameters(CharacterView.Class);
    Vector(1.f, 1.f, 1.f, object.Light);
    if (parameters[0] != 0)
        Vector(parameters[1], parameters[2], parameters[3], object.Angle);
    object.Scale = parameters[4];
    VectorCopy(m_createBasePosition, object.Position);
    object.Position[0] += parameters[5];
    object.Position[2] += parameters[6];
}
void CCharMakeWin::RenderCreateCharacter()
{
    const auto viewport = panel_.PreviewBounds();
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    const auto camera = panel_.PreviewCamera();
    vec3_t position, angle;
    Vector(camera[0], camera[1], camera[2], position);
    Vector(camera[3], camera[4], camera[5], angle);
    g_Camera.FOV = camera[6];
    MoveCharacterCamera(m_createBasePosition, position, angle);
    // PreviewBounds() comes from the Rml layout (top-left origin), while the
    // render tape viewport is bottom-left origin (see MapGpuRect in the GPU
    // backend). Without this flip the preview is mirrored to the bottom of the
    // screen.
    BeginOpengl(viewport.x, REFERENCE_HEIGHT - viewport.y - viewport.height, viewport.width,
                viewport.height);
    sessionKeeper_.Renderer()->RenderCharacter(&CharacterView, &CharacterView.Object, 0,
                                               &m_createVisual);
    glViewport2(0, 0, WindowWidth, WindowHeight);
    EndOpengl();
}

//*****************************************************************************
//*****************************************************************************

void SessionUiUnit::RenderAccountBlockMessage()
{
    g_RenderText.SetTextColor(0, 0, 0, 255);
    g_RenderText.SetBgColor(255, 255, 0, 128);
    g_RenderText.RenderText(CharacterSelectionDetail::kAccountBlockMsgX,
                            CharacterSelectionDetail::kAccountBlockPrimaryY,
                            I18N::Game::ThisAccountIsItemBlocked, 0, 0, RT3_WRITE_CENTER);
    g_RenderText.RenderText(CharacterSelectionDetail::kAccountBlockMsgX,
                            CharacterSelectionDetail::kAccountBlockSecondaryY,
                            I18N::Game::PleaseCheckOnHttpMuonlineWebzenComSite, 0, 0,
                            RT3_WRITE_CENTER);
}

void CCharSelMainWin::RenderControls()
{
    for (auto &sprite : m_asprBack)
        sprite.Render();

    EnableAlphaTest();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    g_RenderText.SetFont(LegacyFontRole::Fixed);
    g_RenderText.SetTextColor(CLRDW_WHITE);
    g_RenderText.SetBgColor(0);

    if (m_bAccountBlockItem)
        RenderAccountBlockMessage();

    CWin::RenderButtons();
}

//*****************************************************************************
//*****************************************************************************

void CCreditWin::SetPosition()
{
    m_aSpr[CRW_SPR_PIC_L].SetPosition(0, 126);
    m_aSpr[CRW_SPR_PIC_R].SetPosition(400, 126);
    m_aSpr[CRW_SPR_LOGO].SetPosition(241, 549);

    int nBaseY = int(527.0f / 600.0f * static_cast<float>(WindowHeight));
    m_aSpr[CRW_SPR_DECO].SetPosition(static_cast<int>(WindowWidth) -
                                         m_aSpr[CRW_SPR_DECO].GetWidth(),
                                     nBaseY - m_aSpr[CRW_SPR_DECO].GetHeight());

    for (int i = CRW_SPR_TXT_HIDE0; i <= CRW_SPR_TXT_HIDE2; ++i)
        m_aSpr[i].SetPosition(0, 42 * (i - CRW_SPR_TXT_HIDE0));

    m_btnClose.SetPosition(m_aSpr[CRW_SPR_DECO].GetXPos() + 122,
                           m_aSpr[CRW_SPR_DECO].GetYPos() + 63);
}

void CCreditWin::Show(bool bShow)
{
    if (bShow && !textReady_)
        return;
    CWin::Show(bShow);

    for (int i = 0; i < CRW_SPR_MAX; ++i)
        m_aSpr[i].Show(bShow);

    m_btnClose.Show(bShow);

    if (bShow)
        Init();
    else
        m_eIllustState = HIDE;
}

void CCreditWin::RenderControls()
{
    glDisable(GL_ALPHA_TEST);

    for (int i = 0; i <= CRW_SPR_LOGO; ++i)
        m_aSpr[i].Render();

    long lScreenWidth = static_cast<long>(WindowWidth);
    int nTextBoxWidth;

    g_RenderText.SetFont(LegacyFontRole::Fixed);
    g_RenderText.SetTextColor(CLRDW_BR_GRAY);
    g_RenderText.SetBgColor(0);
    nTextBoxWidth = lScreenWidth / g_fScreenRate_x;

    auto renderCentered = [&](const SCreditItem &item, int x, int y, int width) {
        wchar_t buffer[CRW_NAME_MAX]{};
        CreditsDetail::CopyNameToWide(item.szName, buffer);
        g_RenderText.RenderText(x, y, buffer, width, 0, RT3_SORT_CENTER);
    };

    renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_DEPARTMENT]], 0, 20, nTextBoxWidth);
    renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_TEAM]], 0, 46, nTextBoxWidth);

    g_RenderText.SetTextColor(CLRDW_BR_YELLOW);

    switch (m_nNameCount)
    {
    case 1:
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME0]], 0, 72, nTextBoxWidth);
        break;
    case 2:
        nTextBoxWidth = lScreenWidth / 4 / g_fScreenRate_x;
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME0]], 160, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME1]], 320, 72, nTextBoxWidth);
        break;
    case 3:
        nTextBoxWidth = lScreenWidth / 3 / g_fScreenRate_x;
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME0]], 0, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME1]], 213, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME2]], 426, 72, nTextBoxWidth);
        break;
    case 4:
        nTextBoxWidth = lScreenWidth / 4 / g_fScreenRate_x;

        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME0]], 0, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME1]], 160, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME2]], 320, 72, nTextBoxWidth);
        renderCentered(m_aCredit[m_anTextIndex[CRW_INDEX_NAME3]], 480, 72, nTextBoxWidth);
        break;
    }

    for (int i = CRW_SPR_TXT_HIDE0; i <= CRW_SPR_TXT_HIDE2; ++i)
        m_aSpr[i].Render();

    glEnable(GL_ALPHA_TEST);

    CWin::RenderButtons();
}

void CCreditWin::ReleaseIllustrations() noexcept
{
    for (int i = 0; i < 2; ++i)
    {
        if (ownedIllustrations_[i])
            DeleteBitmap(BITMAP_TEMP + i);
        ownedIllustrations_[i] = false;
    }
}

bool CCreditWin::PrepareText()
{
    if (textReady_)
        return true;
#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&std::fclose)> file(
        std::fopen(CreditsDetail::kCreditDataPath.data(), "rb"), &std::fclose);
#else
    // The path is Windows-spelled (backslashes, mixed case); resolve it against
    // the case-sensitive filesystem.
    std::unique_ptr<FILE, decltype(&std::fclose)> file(
        std::fopen(MuResolvePath(CreditsDetail::kCreditDataPath.data()).c_str(), "rb"),
        &std::fclose);
#endif
    if (!file)
    {
        wchar_t szMessage[256];
        std::swprintf(szMessage, std::size(szMessage), L"%hs file not found.\n",
                      CreditsDetail::kCreditDataPath.data());
        g_ErrorReport.Write(szMessage);
        return false;
    }

    const std::size_t nSize = sizeof(SCreditItem) * CRW_ITEM_MAX;
    auto candidate = std::make_unique<SCreditItem[]>(CRW_ITEM_MAX);
    if (std::fread(candidate.get(), nSize, 1, file.get()) != 1 || std::fgetc(file.get()) != EOF)
    {
        wchar_t szMessage[256];
        std::swprintf(szMessage, std::size(szMessage),
                      L"Failed to read %hs file or file is corrupt.\n",
                      CreditsDetail::kCreditDataPath.data());
        g_ErrorReport.Write(szMessage);
        return false;
    }
    ::BuxConvert(reinterpret_cast<BYTE *>(candidate.get()), static_cast<int>(nSize));
    for (int i = 0; i < CRW_ITEM_MAX; ++i)
    {
        const auto *tail = reinterpret_cast<const BYTE *>(&candidate[i]);
        const auto tailBytes = (CRW_ITEM_MAX - i) * sizeof(SCreditItem);
        constexpr BYTE LegacyDebugFill = 0xCD;
        if (i != 0 && candidate[i].byClass == LegacyDebugFill &&
            std::all_of(tail, tail + tailBytes, [](BYTE byte) { return byte == LegacyDebugFill; }))
            std::fill_n(candidate.get() + i, CRW_ITEM_MAX - i, SCreditItem{});
        const auto &item = candidate[i];
        if (item.byClass == 0)
        {
            std::copy_n(candidate.get(), CRW_ITEM_MAX, m_aCredit);
            textReady_ = true;
            return true;
        }
        if (item.byClass > 3 || std::find(std::begin(item.szName), std::end(item.szName), '\0') ==
                                    std::end(item.szName))
            break;
    }
    g_ErrorReport.Write(L"Invalid credit.bmd text records.\r\n");
    return false;
}

// Two-button confirmation, modelled on the game's other OK/Cancel dialogs (e.g.
// the guild-request box). A bold yellow "WARNING!!!" header sits above the
// orange message body.

bool RememberPasswordDetail::CRememberPasswordMsgBoxLayout::SetLayout()
{
    SEASON3B::CNewUICommonMessageBox *pMsgBox = GetMsgBox();
    if (pMsgBox == nullptr)
        return false;

    if (!pMsgBox->Create(SEASON3B::MSGBOX_COMMON_TYPE_OKCANCEL))
        return false;

    pMsgBox->AddMsg(I18N::Game::LoginSavePasswordWarningTitle, CLRDW_YELLOW,
                    SEASON3B::MSGBOX_FONT_BOLD);
    pMsgBox->AddMsg(I18N::Game::LoginSavePasswordWarningBody, CLRDW_ORANGE);

    pMsgBox->AddCallbackFunc(
        BindCallback(this, &RememberPasswordDetail::CRememberPasswordMsgBoxLayout::OnOk),
        SEASON3B::MSGBOX_EVENT_USER_COMMON_OK);
    pMsgBox->AddCallbackFunc(
        BindCallback(this, &RememberPasswordDetail::CRememberPasswordMsgBoxLayout::OnCancel),
        SEASON3B::MSGBOX_EVENT_USER_COMMON_CANCEL);
    pMsgBox->AddCallbackFunc(
        BindCallback(this, &RememberPasswordDetail::CRememberPasswordMsgBoxLayout::OnOk),
        SEASON3B::MSGBOX_EVENT_PRESSKEY_RETURN);
    pMsgBox->AddCallbackFunc(
        BindCallback(this, &RememberPasswordDetail::CRememberPasswordMsgBoxLayout::OnCancel),
        SEASON3B::MSGBOX_EVENT_PRESSKEY_ESC);
    return true;
}

// namespace
