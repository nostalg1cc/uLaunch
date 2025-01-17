#include <ui/ui_StartupLayout.hpp>
#include <os/os_Account.hpp>
#include <util/util_Convert.hpp>
#include <ui/ui_QMenuApplication.hpp>
#include <fs/fs_Stdio.hpp>

extern ui::QMenuApplication::Ref qapp;
extern cfg::ProcessedTheme theme;

namespace ui
{
    StartupLayout::StartupLayout()
    {
        this->SetBackgroundImage(cfg::ProcessedThemeResource(theme, "ui/Background.png"));

        pu::ui::Color textclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("text_color", "#e1e1e1ff"));
        pu::ui::Color menufocusclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("menu_focus_color", "#5ebcffff"));
        pu::ui::Color menubgclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("menu_bg_color", "#0094ffff"));

        this->infoText = pu::ui::elm::TextBlock::New(0, 100, "Welcome! Please select an account to use.");
        this->infoText->SetColor(textclr);
        this->infoText->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        qapp->ApplyConfigForElement("startup_menu", "info_text", this->infoText);
        this->Add(this->infoText);

        this->usersMenu = pu::ui::elm::Menu::New(400, 160, 480, menubgclr, 100, 4);
        this->usersMenu->SetOnFocusColor(menufocusclr);
        this->Add(this->usersMenu);
    }

    void StartupLayout::user_Click()
    {
        auto usridx = this->usersMenu->GetSelectedIndex() - 1;
        auto uid = this->userlist[usridx];
        auto haspass = this->passlist[usridx];
        bool login_ok = false;
        if(haspass)
        {
            SwkbdConfig swkbd;
            swkbdCreate(&swkbd, 0);
            swkbdConfigMakePresetPassword(&swkbd);
            swkbdConfigSetStringLenMax(&swkbd, 15);
            swkbdConfigSetGuideText(&swkbd, "User password");
            swkbdConfigSetHeaderText(&swkbd, "Input user password");
            char inpass[0x10] = {0};
            auto rc = swkbdShow(&swkbd, inpass, 0x10);
            swkbdClose(&swkbd);
            if(R_SUCCEEDED(rc))
            {
                auto [rc2, pass] = db::PackPassword(uid, inpass);
                rc = rc2;
                if(R_SUCCEEDED(rc))
                {
                    am::QMenuCommandWriter writer(am::QDaemonMessage::TryLogUser);
                    writer.Write<db::PassBlock>(pass);
                    writer.FinishWrite();

                    am::QMenuCommandResultReader reader;
                    rc = reader.GetReadResult();
                    reader.FinishRead();

                    if(R_FAILED(rc)) qapp->CreateShowDialog("Login", "Invalid password. Please try again.", {"Ok"}, true);
                    else login_ok = true;
                }
            }
        }
        else login_ok = true;
        if(login_ok)
        {
            qapp->FadeOut();
            qapp->SetSelectedUser(uid);
            qapp->LoadMenu();
            qapp->StartPlayBGM();
            qapp->FadeIn();
        }
    }

    void StartupLayout::create_Click()
    {
        u8 in[0xa0] = {0};
        u8 out[0x18] = {0};
        *(u32*)in = 1; // PselUiMode_UserCreation
        LibAppletArgs args;
        libappletArgsCreate(&args, 0);
        auto rc = libappletLaunch(AppletId_playerSelect, &args, in, 0xa0, out, 0x18, NULL);
        if(R_SUCCEEDED(rc))
        {
            rc = *(u32*)out;
            if(R_SUCCEEDED(rc))
            {
                qapp->FadeOut();
                this->ReloadMenu();
                qapp->FadeIn();
            }
        }
    }

    void StartupLayout::ReloadMenu()
    {
        pu::ui::Color textclr = pu::ui::Color::FromHex(qapp->GetUIConfigValue<std::string>("text_color", "#e1e1e1ff"));
        this->usersMenu->ClearItems();
        this->userlist.clear();
        this->passlist.clear();
        this->usersMenu->SetVisible(true);
        
        auto citm = pu::ui::elm::MenuItem::New("Create new user");
        citm->SetColor(textclr);
        citm->AddOnClick(std::bind(&StartupLayout::create_Click, this));
        this->usersMenu->AddItem(citm);
        auto [rc, users] = os::QuerySystemAccounts(true);
        if(R_SUCCEEDED(rc))
        {
            for(auto user: users)
            {
                auto [rc, name] = os::GetAccountName(user);
                if(R_FAILED(rc)) continue;

                am::QMenuCommandWriter writer(am::QDaemonMessage::UserHasPassword);
                writer.Write<u128>(user);
                writer.FinishWrite();

                am::QMenuCommandResultReader res;
                res.FinishRead();

                bool has_pass = R_SUCCEEDED(res.GetReadResult());
                if(has_pass) name += " (password)";

                auto path = os::GetIconCacheImagePath(user);
                auto uitm = pu::ui::elm::MenuItem::New(name);
                uitm->SetIcon(path);
                uitm->AddOnClick(std::bind(&StartupLayout::user_Click, this));
                uitm->SetColor(textclr);

                this->userlist.push_back(user);
                this->passlist.push_back(has_pass);

                this->usersMenu->AddItem(uitm);
            }
        }
        else
        {
            this->usersMenu->SetVisible(false);
            this->infoText->SetText("Unable to obtain system accounts. Hold power to power off or reboot the console.");
        }
    }
}