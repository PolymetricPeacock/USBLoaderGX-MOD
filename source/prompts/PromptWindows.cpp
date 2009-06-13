#include <gccore.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "usbloader/wbfs.h"
#include "usbloader/wdvd.h"
#include "usbloader/partition.h"
#include "usbloader/usbstorage.h"
#include "usbloader/getentries.h"
#include "language/language.h"
#include "libwiigui/gui.h"
#include "libwiigui/gui_diskcover.h"
#include "network/updater.h"
#include "network/http.h"
#include "mload/mload.h"
#include "fatmounter.h"
#include "listfiles.h"
#include "menu.h"
#include "filelist.h"
#include "sys.h"
#include "wpad.h"

/*** Variables that are also used extern ***/
int cntMissFiles = 0;

/*** Variables used only in this file ***/
static GuiText prTxt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
static GuiText timeTxt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
static GuiText sizeTxt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
static GuiImageData progressbar(progressbar_png);
static GuiImage progressbarImg(&progressbar);
static char missingFiles[500][12];

/*** Extern variables ***/
extern GuiWindow * mainWindow;
extern GuiSound * bgMusic;
extern u32 gameCnt;
extern s32 gameSelected, gameStart;
extern float gamesize;
extern struct discHdr * gameList;
extern u8 shutdown;
extern u8 reset;

/*** Extern functions ***/
extern void ResumeGui();
extern void HaltGui();

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
int OnScreenKeyboard(char * var, u32 maxlen, int min)
{
	int save = -1;
	int keyset = 0;
	if (Settings.keyset == us) keyset = 0;
	else if (Settings.keyset == dvorak) keyset = 1;
	else if (Settings.keyset == euro) keyset = 2;
	else if (Settings.keyset == azerty) keyset = 3;

	GuiKeyboard keyboard(var, maxlen, min, keyset);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[50];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigB;
	trigB.SetSimpleTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);

	GuiText okBtnTxt(LANGUAGE.ok, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage okBtnImg(&btnOutline);
	if (Settings.wsprompt == yes){
	okBtnTxt.SetWidescreen(CFG.widescreen);
	okBtnImg.SetWidescreen(CFG.widescreen);
	}
	GuiButton okBtn(&okBtnImg,&okBtnImg, 0, 4, 5, 15, &trigA, &btnSoundOver, &btnClick,1);
	okBtn.SetLabel(&okBtnTxt);
	GuiText cancelBtnTxt(LANGUAGE.Cancel, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage cancelBtnImg(&btnOutline);
	if (Settings.wsprompt == yes){
	cancelBtnTxt.SetWidescreen(CFG.widescreen);
	cancelBtnImg.SetWidescreen(CFG.widescreen);
	}
	GuiButton cancelBtn(&cancelBtnImg,&cancelBtnImg, 1, 4, -5, 15, &trigA, &btnSoundOver, &btnClick,1);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetTrigger(&trigB);

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		VIDEO_WaitVSync();

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return save;
}

/****************************************************************************
 * WindowCredits
 * Display credits
 ***************************************************************************/
void WindowCredits()
{
	int angle = 0;
	GuiSound * creditsMusic = NULL;

    s32 thetimeofbg = bgMusic->GetPlayTime();
	StopOgg();

	creditsMusic = new GuiSound(credits_music_ogg, credits_music_ogg_size, SOUND_OGG, 55);
	creditsMusic->SetVolume(60);
	creditsMusic->SetLoop(1);
	creditsMusic->Play();

	bool exit = false;
	int i = 0;
	int y = 20;

	GuiWindow creditsWindow(screenwidth,screenheight);
	GuiWindow creditsWindowBox(580,448);
	creditsWindowBox.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);

	GuiImageData creditsBox(credits_bg_png);
	GuiImage creditsBoxImg(&creditsBox);
	creditsBoxImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	creditsWindowBox.Append(&creditsBoxImg);

	GuiImageData star(little_star_png);
	GuiImage starImg(&star);
	starImg.SetWidescreen(CFG.widescreen); //added
	starImg.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	starImg.SetPosition(505,350);

	int numEntries = 20;
	GuiText * txt[numEntries];

	txt[i] = new GuiText(LANGUAGE.Credits, 26, (GXColor){255, 255, 255, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,12); i++;

	char SvnRev[10];
	snprintf(SvnRev, 10, "Rev%s", SVN_REV);

	txt[i] = new GuiText(SvnRev, 18, (GXColor){255, 255, 255, 255});
	txt[i]->SetAlignment(ALIGN_RIGHT, ALIGN_TOP); txt[i]->SetPosition(-30,y); i++; y+=34;

	txt[i] = new GuiText("USB Loader GX", 24, (GXColor){255, 255, 255, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(0,y); i++; y+=26;

	txt[i] = new GuiText(": http://code.google.com/p/usbloader-gui/", 20, (GXColor){255, 255, 255, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(50,y); i++; //y+=28;

	txt[i] = new GuiText(LANGUAGE.OfficialSite, 20, (GXColor){255, 255, 255, 255});
	txt[i]->SetAlignment(ALIGN_CENTRE, ALIGN_TOP); txt[i]->SetPosition(-180,y); i++; y+=28;

	GuiText::SetPresets(22, (GXColor){255, 255, 255,  255}, 0, GuiText::WRAP,
			FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP, ALIGN_LEFT, ALIGN_TOP);

	txt[i] = new GuiText("Coding:");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(70,y);
	i++;

	txt[i] = new GuiText("dimok / nIxx");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=24;

	txt[i] = new GuiText("hungyip84 / giantpune");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=24;

	txt[i] = new GuiText("ardi / DrayX7");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=34;

	txt[i] = new GuiText("Design:");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(70,y);
	i++;

	txt[i] = new GuiText("cyrex / NeoRame / WiiShizzza");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=20;

	txt[i] = new GuiText(" ");
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=22;

	txt[i] = new GuiText(LANGUAGE.Thanksto);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(70,y);
	i++;

    char text[100];
    sprintf(text, "djtaz %s", LANGUAGE.Forhostingcovers);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=24;

    sprintf(text, "CorneliousJD %s", LANGUAGE.Forhostingupdatefiles);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(220,y);
	i++;
	y+=30;

	txt[i] = new GuiText(LANGUAGE.Specialthanksto);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(70,y);
	i++;
	y+=24;

    sprintf(text, "Waninkoko, Kwiirk & Hermes %s", LANGUAGE.theUSBLoaderandreleasingthesourcecode);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(100,y);
	i++;
	y+=22;

    sprintf(text, "Tantric %s LibWiiGui", LANGUAGE.awesometool);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(100,y);
	i++;
	y+=22;

    sprintf(text, "Fishears/Nuke %s Ocarina", LANGUAGE.For);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(100,y);
	i++;
	y+=22;

    sprintf(text, "WiiPower %s", LANGUAGE.diversepatches);
	txt[i] = new GuiText(text);
	txt[i]->SetAlignment(ALIGN_LEFT, ALIGN_TOP); txt[i]->SetPosition(100,y);
	i++;
	y+=22;

	for(i=0; i < numEntries; i++)
		creditsWindowBox.Append(txt[i]);


	creditsWindow.Append(&creditsWindowBox);
	creditsWindow.Append(&starImg);

    creditsWindow.SetEffect(EFFECT_FADE, 30);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&creditsWindow);
	ResumeGui();

	while(!exit)
	{
		angle++;
		if(angle > 360)
		angle = 0;
        usleep(12000);
		starImg.SetAngle(angle);
		if(ButtonsPressed() != 0)
		exit = true;

	}

	creditsMusic->Stop();

	delete creditsMusic;

    creditsWindow.SetEffect(EFFECT_FADE, -30);
	while(creditsWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&creditsWindow);
	mainWindow->SetState(STATE_DEFAULT);
	for(i=0; i < numEntries; i++) {
		delete txt[i];
		txt[i] = NULL;
	}
	ResumeGui();

	if(!strcmp("", Settings.oggload_path) || !strcmp("notset", Settings.ogg_path)) {
        bgMusic->Play();
    } else {
        bgMusic->PlayOggFile(Settings.ogg_path);
    }
    bgMusic->SetPlayTime(thetimeofbg);
    SetVolumeOgg(255*(Settings.volume/100.0));
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice of up to 4 Buttons.
 *
 * Give him 1 Titel, 1 Subtitel and 4 Buttons
 * If titel/subtitle or one of the buttons is not needed give him a 0 on that
 * place.
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label,
                const char *btn2Label, const char *btn3Label,
                const char *btn4Label)
{
	int choice = -1;

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);
	char imgPath[50];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);


	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigB;
	trigB.SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);
	}

	GuiText titleTxt(title, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,55);
	GuiText msgTxt(msg, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-40);
	msgTxt.SetMaxWidth(430);

	GuiText btn1Txt(btn1Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn1Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn1Txt.SetWidescreen(CFG.widescreen);
	btn1Img.SetWidescreen(CFG.widescreen);
	}

	GuiButton btn1(&btn1Img, &btn1Img, 0,3,0,0,&trigA,&btnSoundOver,&btnClick,1);
	btn1.SetLabel(&btn1Txt);
	btn1.SetState(STATE_SELECTED);

	GuiText btn2Txt(btn2Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn2Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn2Txt.SetWidescreen(CFG.widescreen);
	btn2Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn2(&btn2Img, &btn2Img, 0,3,0,0,&trigA,&btnSoundOver,&btnClick,1);
	btn2.SetLabel(&btn2Txt);
	if(!btn3Label && !btn4Label)
	btn2.SetTrigger(&trigB);

    GuiText btn3Txt(btn3Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn3Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn3Txt.SetWidescreen(CFG.widescreen);
	btn3Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn3(&btn3Img, &btn3Img, 0,3,0,0,&trigA,&btnSoundOver,&btnClick,1);
	btn3.SetLabel(&btn3Txt);
	if(!btn4Label)
	btn3.SetTrigger(&trigB);

    GuiText btn4Txt(btn4Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn4Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn4Txt.SetWidescreen(CFG.widescreen);
	btn4Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn4(&btn4Img, &btn4Img, 0,3,0,0,&trigA,&btnSoundOver,&btnClick,1);
	btn4.SetLabel(&btn4Txt);
	if(btn4Label)
	btn4.SetTrigger(&trigB);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust buttons for widescreen
		msgTxt.SetMaxWidth(330);

		if(btn2Label && !btn3Label && !btn4Label)
        {
            btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(70, -80);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-70, -80);
            btn3.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn3.SetPosition(-70, -55);
            btn4.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn4.SetPosition(70, -55);
        } else if(btn2Label && btn3Label && !btn4Label) {
            btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(70, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-70, -120);
            btn3.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn3.SetPosition(0, -55);
            btn4.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn4.SetPosition(70, -55);
        } else if(btn2Label && btn3Label && btn4Label) {
            btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(70, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-70, -120);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(70, -55);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-70, -55);
        }   else if(!btn2Label && btn3Label && btn4Label) {
            btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn1.SetPosition(0, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-70, -120);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(70, -55);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-70, -55);
        } else {
            btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn1.SetPosition(0, -80);
            btn2.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn2.SetPosition(70, -120);
            btn3.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn3.SetPosition(-70, -55);
            btn4.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn4.SetPosition(70, -55);
        }
	} else {

	    if(btn2Label && !btn3Label && !btn4Label) {
            btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(40, -45);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-40, -45);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(50, -65);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-50, -65);
	    } else if(btn2Label && btn3Label && !btn4Label) {
            btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(50, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-50, -120);
            btn3.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn3.SetPosition(0, -65);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-50, -65);
	    } else if(btn2Label && btn3Label && btn4Label) {
	        btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn1.SetPosition(50, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-50, -120);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(50, -65);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-50, -65);
	    } else if(!btn2Label && btn3Label && btn4Label) {
	        btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn1.SetPosition(0, -120);
            btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn2.SetPosition(-50, -120);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(50, -65);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-50, -65);
	    } else {
	        btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
            btn1.SetPosition(0, -45);
            btn2.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn2.SetPosition(50, -120);
            btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
            btn3.SetPosition(50, -65);
            btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
            btn4.SetPosition(-50, -65);
	    }

	}

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	if(btn1Label)
	promptWindow.Append(&btn1);
	if(btn2Label)
		promptWindow.Append(&btn2);
    if(btn3Label)
		promptWindow.Append(&btn3);
    if(btn4Label)
		promptWindow.Append(&btn4);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		VIDEO_WaitVSync();
		if(shutdown == 1)
		{
			wiilight(0);
			Sys_Shutdown();
		}
		if(reset == 1)
			Sys_Reboot();
		if(btn1.GetState() == STATE_CLICKED) {
			choice = 1;
		}
		else if(btn2.GetState() == STATE_CLICKED) {
		    if(!btn3Label)
			choice = 0;
			else
			choice = 2;
		}
		else if(btn3.GetState() == STATE_CLICKED) {
		    if(!btn4Label)
			choice = 0;
			else
			choice = 3;
		}
		else if(btn4.GetState() == STATE_CLICKED) {
			choice = 0;
		}
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * WindowExitPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice of up to 4 Buttons.
 *
 * Give him 1 Titel, 1 Subtitel and 4 Buttons
 * If titel/subtitle or one of the buttons is not needed give him a 0 on that
 * place.
 ***************************************************************************/
int
WindowExitPrompt(const char *title, const char *msg, const char *btn1Label,
                const char *btn2Label, const char *btn3Label,
                const char *btn4Label)
{
    GuiSound * homein = NULL;
    homein = new GuiSound(menuin_ogg, menuin_ogg_size, SOUND_OGG, Settings.sfxvolume);
    homein->SetVolume(Settings.sfxvolume);
	homein->SetLoop(0);
	homein->Play();

	GuiSound * homeout = NULL;
    homeout = new GuiSound(menuout_ogg, menuout_ogg_size, SOUND_OGG, Settings.sfxvolume);
    homeout->SetVolume(Settings.sfxvolume);
	homeout->SetLoop(0);

    int choice = -1;
	char imgPath[100];
	u8 HBC=0;
	GuiWindow promptWindow(640,480);
	promptWindow.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	promptWindow.SetPosition(0, 0);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	GuiImageData top(exit_top_png);
	GuiImageData topOver(exit_top_over_png);
	GuiImageData bottom(exit_bottom_png);
	GuiImageData bottomOver(exit_bottom_over_png);
	GuiImageData button(exit_button_png);
	GuiImageData wiimote(wiimote_png);
	GuiImageData close(closebutton_png);

	snprintf(imgPath, sizeof(imgPath), "%sbattery_white.png", CFG.theme_path);
	GuiImageData battery(imgPath, battery_white_png);
	snprintf(imgPath, sizeof(imgPath), "%sbattery_red.png", CFG.theme_path);
	GuiImageData batteryRed(imgPath, battery_red_png);
	snprintf(imgPath, sizeof(imgPath), "%sbattery_bar_white.png", CFG.theme_path);
	GuiImageData batteryBar(imgPath, battery_bar_white_png);

	#ifdef HW_RVL
	int i = 0, ret = 0, level;
	char txt[3];
	GuiText * batteryTxt[4];
	GuiImage * batteryImg[4];
	GuiImage * batteryBarImg[4];
	GuiButton * batteryBtn[4];

	for(i=0; i < 4; i++)
	{

		if(i == 0)
			sprintf(txt, "P%d", i+1);
		else
			sprintf(txt, "P%d", i+1);

		batteryTxt[i] = new GuiText(txt, 22, (GXColor){255,255,255, 255});
		batteryTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i] = new GuiImage(&battery);
		batteryImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryImg[i]->SetPosition(36, 0);
		batteryImg[i]->SetTile(0);
		batteryBarImg[i] = new GuiImage(&batteryBar);
		batteryBarImg[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBarImg[i]->SetPosition(33, 0);

		batteryBtn[i] = new GuiButton(40, 20);
		batteryBtn[i]->SetLabel(batteryTxt[i]);
		batteryBtn[i]->SetImage(batteryBarImg[i]);
		batteryBtn[i]->SetIcon(batteryImg[i]);
		batteryBtn[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		batteryBtn[i]->SetRumble(false);
		batteryBtn[i]->SetAlpha(70);
		batteryBtn[i]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 50);
	}

	batteryBtn[0]->SetPosition(180,150);
	batteryBtn[1]->SetPosition(284, 150);
	batteryBtn[2]->SetPosition(388, 150);
	batteryBtn[3]->SetPosition(494, 150);


    char * sig = (char *)0x80001804;
    if(
        sig[0] == 'S' &&
        sig[1] == 'T' &&
        sig[2] == 'U' &&
        sig[3] == 'B' &&
        sig[4] == 'H' &&
        sig[5] == 'A' &&
        sig[6] == 'X' &&
        sig[7] == 'X')
        HBC=1; // Exit to HBC
    #endif

    GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigB;
	trigB.SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText titleTxt(LANGUAGE.Homemenu, 36, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(-180,40);
	titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);

	GuiText closeTxt(LANGUAGE.Close, 28, (GXColor){0, 0, 0, 255});
	closeTxt.SetPosition(10,3);
	GuiImage closeImg(&close);
	if (Settings.wsprompt == yes){
	closeTxt.SetWidescreen(CFG.widescreen);
	closeImg.SetWidescreen(CFG.widescreen);
	}
	GuiButton closeBtn(close.GetWidth(), close.GetHeight());
	closeBtn.SetImage(&closeImg);
	closeBtn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	closeBtn.SetPosition(190,30);
	closeBtn.SetLabel(&closeTxt);
	closeBtn.SetRumble(false);
	closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);

	GuiImage btn1Img(&top);
	GuiImage btn1OverImg(&topOver);
	GuiButton btn1(&btn1Img,&btn1OverImg, 0, 3, 0, 0, &trigA, &btnSoundOver, &btnClick,0);
	btn1.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);

	GuiText btn2Txt(btn1Label, 28, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&button);
	if (Settings.wsprompt == yes){
	btn2Txt.SetWidescreen(CFG.widescreen);
	btn2Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn2(&btn2Img,&btn2Img, 2, 5, -150, 0, &trigA, &btnSoundOver, &btnClick,1);
	btn2.SetLabel(&btn2Txt);
	btn2.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_IN, 50);
	btn2.SetRumble(false);
	if (HBC==1){btn2.SetPosition(-150, 0);}

    GuiText btn3Txt(btn2Label, 28, (GXColor){0, 0, 0, 255});
	GuiImage btn3Img(&button);
	if (Settings.wsprompt == yes){
	btn3Txt.SetWidescreen(CFG.widescreen);
	btn3Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn3(&btn3Img,&btn3Img, 2, 5, 150, 0, &trigA, &btnSoundOver, &btnClick,1);
	btn3.SetLabel(&btn3Txt);
	btn3.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_IN, 50);
	btn3.SetRumble(false);
	if (HBC==1){btn3.SetPosition(150, 0);}
	else {btn3.SetPosition(0,0);}

	GuiImage btn4Img(&bottom);
	GuiImage btn4OverImg(&bottomOver);
	GuiButton btn4(&btn4Img,&btn4OverImg, 0, 4, 0, 0, &trigA, &btnSoundOver, &btnClick,0);
	btn4.SetTrigger(&trigB);
	btn4.SetTrigger(&trigHome);
	btn4.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 50);

	GuiImage wiimoteImg(&wiimote);
	if (Settings.wsprompt == yes){wiimoteImg.SetWidescreen(CFG.widescreen);}
	wiimoteImg.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	wiimoteImg.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_IN, 50);
	wiimoteImg.SetPosition(50,210);

	if (HBC==1){promptWindow.Append(&btn2);}
    promptWindow.Append(&btn3);
    promptWindow.Append(&btn4);
    promptWindow.Append(&btn1);
	promptWindow.Append(&closeBtn);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&wiimoteImg);

	#ifdef HW_RVL
	promptWindow.Append(batteryBtn[0]);
    promptWindow.Append(batteryBtn[1]);
    promptWindow.Append(batteryBtn[2]);
    promptWindow.Append(batteryBtn[3]);
    #endif

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		VIDEO_WaitVSync();

		#ifdef HW_RVL
		for(i=0; i < 4; i++)
		{
			if(WPAD_Probe(i, NULL) == WPAD_ERR_NONE) // controller connected
			{
				level = (userInput[i].wpad.battery_level / 100.0) * 4;
				if(level > 4) level = 4;
				batteryImg[i]->SetTile(level);

				if(level == 0)
					batteryBarImg[i]->SetImage(&batteryRed);
				else
					batteryBarImg[i]->SetImage(&batteryBar);

				batteryBtn[i]->SetAlpha(255);
			}
			else // controller not connected
			{
				batteryImg[i]->SetTile(0);
				batteryImg[i]->SetImage(&battery);
				batteryBtn[i]->SetAlpha(70);
			}
		}
		#endif


		if(shutdown == 1)
		{
			wiilight(0);
			Sys_Shutdown();
		}
		if(reset == 1)
			Sys_Reboot();
		if(btn1.GetState() == STATE_CLICKED) {
			choice = 1;
			btn1.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			btn4.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
            btn2.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
            btn3.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			wiimoteImg.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
			#ifdef HW_RVL
			for (int i = 0; i < 4; i++)
			batteryBtn[i]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
            #endif
		}
		else if(btn4.GetState() == STATE_SELECTED)
		{
			wiimoteImg.SetPosition(50,165);
		}
		else if(btn2.GetState() == STATE_CLICKED) {
            ret = WindowPrompt(LANGUAGE.Areyousure, 0, LANGUAGE.Yes, LANGUAGE.No, 0, 0);
			if (ret == 1) {
			choice = 2;
			}
			HaltGui();
            mainWindow->SetState(STATE_DISABLED);
			promptWindow.SetState(STATE_DEFAULT);
            mainWindow->ChangeFocus(&promptWindow);
			ResumeGui();
			btn2.ResetState();
		}
		else if(btn3.GetState() == STATE_CLICKED) {
			ret = WindowPrompt(LANGUAGE.Areyousure, 0, LANGUAGE.Yes, LANGUAGE.No, 0, 0);
			if (ret == 1) {
			choice = 3;
			}
			HaltGui();
			mainWindow->SetState(STATE_DISABLED);
			promptWindow.SetState(STATE_DEFAULT);
			mainWindow->ChangeFocus(&promptWindow);
			ResumeGui();
			btn3.ResetState();
		}
		else if(btn4.GetState() == STATE_CLICKED) {
		    btn1.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			closeBtn.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			btn4.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
			btn2.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
            btn3.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
			titleTxt.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			wiimoteImg.SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
			#ifdef HW_RVL
			for (int i = 0; i < 4; i++)
			batteryBtn[i]->SetEffect(EFFECT_SLIDE_BOTTOM | EFFECT_SLIDE_OUT, 50);
            #endif
			choice = 0;
		}
		else if(btn4.GetState() != STATE_SELECTED)
		{
			wiimoteImg.SetPosition(50,210);
		}
	}
    homeout->Play();
    while(btn1.GetEffect() > 0) usleep(50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	homein->Stop();
	delete homein;
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
    while(homeout->IsPlaying() > 0) usleep(50);
    homeout->Stop();
	delete homeout;
	ResumeGui();
	return choice;
}

/****************************************************************************
 * GameWindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int GameWindowPrompt()
{
	int choice = -1, angle = 0;
	f32 size = 0.0;
	char ID[5];
	char IDFull[7];

	u8 faveChoice = 0;
	u16 playCount = 0;

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);

	snprintf(imgPath, sizeof(imgPath), "%sfavorite.png", CFG.theme_path);
	GuiImageData imgFavorite(imgPath, favorite_png);
	snprintf(imgPath, sizeof(imgPath), "%snot_favorite.png", CFG.theme_path);
	GuiImageData imgNotFavorite(imgPath, not_favorite_png);

	snprintf(imgPath, sizeof(imgPath), "%sstartgame_arrow_left.png", CFG.theme_path);
	GuiImageData imgLeft(imgPath, startgame_arrow_left_png);
	snprintf(imgPath, sizeof(imgPath), "%sstartgame_arrow_right.png", CFG.theme_path);
	GuiImageData imgRight(imgPath, startgame_arrow_right_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigB;
	trigB.SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);
	GuiTrigger trigL;
	trigL.SetButtonOnlyTrigger(-1, WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT, PAD_BUTTON_LEFT);
	GuiTrigger trigR;
	trigR.SetButtonOnlyTrigger(-1, WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT, PAD_BUTTON_RIGHT);
    GuiTrigger trigPlus;
	trigPlus.SetButtonOnlyTrigger(-1, WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_PLUS, 0);
	GuiTrigger trigMinus;
	trigMinus.SetButtonOnlyTrigger(-1, WPAD_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_MINUS, 0);

	if (CFG.widescreen)
		snprintf(imgPath, sizeof(imgPath), "%swdialogue_box_startgame.png", CFG.theme_path);
	else
		snprintf(imgPath, sizeof(imgPath), "%sdialogue_box_startgame.png", CFG.theme_path);

	GuiImageData dialogBox(imgPath, CFG.widescreen ? wdialogue_box_startgame_png : dialogue_box_startgame_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiTooltip nameBtnTT(LANGUAGE.RenameGameonWBFS);
	if (Settings.wsprompt == yes)
		nameBtnTT.SetWidescreen(CFG.widescreen);
	GuiText nameTxt("", 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	if (Settings.wsprompt == yes)
		nameTxt.SetWidescreen(CFG.widescreen);
	nameTxt.SetMaxWidth(350, GuiText::SCROLL);
	GuiButton nameBtn(120,50);
	nameBtn.SetLabel(&nameTxt);
//	nameBtn.SetLabelOver(&nameTxt);
	nameBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	nameBtn.SetPosition(0,-122);
	nameBtn.SetSoundOver(&btnSoundOver);
	nameBtn.SetSoundClick(&btnClick);
	nameBtn.SetToolTip(&nameBtnTT,24,-30, ALIGN_LEFT);

	if (Settings.godmode == 1){
		nameBtn.SetTrigger(&trigA);
		nameBtn.SetEffectGrow();
	}

    GuiText sizeTxt(NULL, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255}); //TODO: get the size here
	sizeTxt.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	sizeTxt.SetPosition(-60,70);

//	GuiImage diskImg;
	GuiDiskCover diskImg;
	diskImg.SetWidescreen(CFG.widescreen);
	diskImg.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	diskImg.SetAngle(angle);
	GuiDiskCover diskImg2;
	diskImg2.SetWidescreen(CFG.widescreen);
	diskImg2.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	diskImg2.SetPosition(0, -20);
	diskImg2.SetAngle(angle);
	diskImg2.SetBeta(180);

	GuiText playcntTxt(NULL, 18, (GXColor){THEME.info_r, THEME.info_g, THEME.info_b, 255});
	playcntTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	playcntTxt.SetPosition(-115,45);

	GuiButton btn1(160, 160);
	btn1.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	btn1.SetPosition(0, -20);
	btn1.SetImage(&diskImg);

	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetSoundClick(&btnClick);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);

	GuiText btn2Txt(LANGUAGE.Back, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn2Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn2Txt.SetWidescreen(CFG.widescreen);
	btn2Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn2(&btn2Img,&btn2Img, 1, 5, 0, 0, &trigA, &btnSoundOver, &btnClick,1);
	if (Settings.godmode == 1)
	{
		btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
		btn2.SetPosition(-50, -40);
	}
	else
	{
		btn2.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn2.SetPosition(0, -40);
	}

	btn2.SetLabel(&btn2Txt);
	btn2.SetTrigger(&trigB);

	GuiText btn3Txt(LANGUAGE.settings, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn3Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn3Txt.SetWidescreen(CFG.widescreen);
	btn3Img.SetWidescreen(CFG.widescreen);}
	GuiButton btn3(&btn3Img,&btn3Img, 0, 4, 50, -40, &trigA, &btnSoundOver, &btnClick,1);
	btn3.SetLabel(&btn3Txt);

	GuiImage btnFavoriteImg;
	btnFavoriteImg.SetWidescreen(CFG.widescreen);
	//GuiButton btnFavorite(&btnFavoriteImg,&btnFavoriteImg, 2, 5, -125, -60, &trigA, &btnSoundOver, &btnClick,1);
	GuiButton btnFavorite(imgFavorite.GetWidth(), imgFavorite.GetHeight());
	btnFavorite.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	btnFavorite.SetPosition(-125, -60);
	btnFavorite.SetImage(&btnFavoriteImg);
	btnFavorite.SetSoundOver(&btnSoundOver);
	btnFavorite.SetSoundClick(&btnClick);
	btnFavorite.SetTrigger(&trigA);
	btnFavorite.SetEffectGrow();

	GuiImage btnLeftImg(&imgLeft);
	if (Settings.wsprompt == yes)
		{
			btnLeftImg.SetWidescreen(CFG.widescreen);
		}
	GuiButton btnLeft(&btnLeftImg,&btnLeftImg, 0, 5, 20, 0, &trigA, &btnSoundOver, &btnClick,1);
	btnLeft.SetTrigger(&trigL);
	btnLeft.SetTrigger(&trigMinus);

	GuiImage btnRightImg(&imgRight);
	if (Settings.wsprompt == yes)
		{
			btnRightImg.SetWidescreen(CFG.widescreen);
		}
	GuiButton btnRight(&btnRightImg,&btnRightImg, 1, 5, -20, 0, &trigA, &btnSoundOver, &btnClick,1);
	btnRight.SetTrigger(&trigR);
	btnRight.SetTrigger(&trigPlus);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&nameBtn);
	promptWindow.Append(&sizeTxt);
	promptWindow.Append(&playcntTxt);
	promptWindow.Append(&btn2);
	promptWindow.Append(&btnLeft);
	promptWindow.Append(&btnRight);
	promptWindow.Append(&btnFavorite);

	//check if unlocked
	if (Settings.godmode == 1)
	{
    promptWindow.Append(&btn3);
	}

	promptWindow.Append(&diskImg2);
	promptWindow.Append(&btn1);

	short changed = -1;
	GuiImageData * diskCover = NULL;
	GuiImageData * diskCover2 = NULL;

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);

	while (changed)
	{
		if (changed == 1){
			promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_IN, 50);
		}
		else if (changed == 2){
			promptWindow.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_IN, 50);
		}
		else if (changed == 3 || changed == 4)
		{
			if(diskCover2)
				delete diskCover2;
			diskCover2 = NULL;
			if(diskCover)
				diskCover2 = diskCover;
			diskCover = NULL;
		}

		//load disc image based or what game is seleted
		struct discHdr * header = &gameList[gameSelected];

		snprintf (ID,sizeof(ID),"%c%c%c", header->id[0], header->id[1], header->id[2]);
		snprintf (IDFull,sizeof(IDFull),"%c%c%c%c%c%c", header->id[0], header->id[1], header->id[2],header->id[3], header->id[4], header->id[5]);

		if (diskCover)
			delete diskCover;

		snprintf(imgPath,sizeof(imgPath),"%s%s.png", Settings.disc_path, IDFull); //changed to current full id
		diskCover = new GuiImageData(imgPath,0);

		if (!diskCover->GetImage())
		{
			delete diskCover;
			snprintf(imgPath, sizeof(imgPath), "%s%s.png", Settings.disc_path, ID); //changed to current id
			diskCover = new GuiImageData(imgPath, 0);


		if (!diskCover->GetImage())
		{	snprintf (ID,sizeof(ID),"%c%c%c%c", header->id[0], header->id[1], header->id[2], header->id[3]);

			delete diskCover;
			snprintf(imgPath, sizeof(imgPath), "%s%s.png", Settings.disc_path, ID); //changed to current id
			diskCover = new GuiImageData(imgPath, 0);
			if (!diskCover->GetImage())
			{
				delete diskCover;
				snprintf(imgPath, sizeof(imgPath), "%snodisc.png", Settings.disc_path); //changed to nodisc.png
				diskCover = new GuiImageData(imgPath,nodisc_png);
			}
		}
		}



		if (changed == 3){
			diskImg.SetImage(diskCover2);
			diskImg.SetBeta(0);
			diskImg.SetBetaRotateEffect(-90, 15);
			diskImg2.SetImage(diskCover);
			diskImg2.SetAngle(diskImg.GetAngle());
			diskImg2.SetBeta(180);
			diskImg2.SetBetaRotateEffect(-90, 15);
			sizeTxt.SetEffect(EFFECT_FADE, -17);
			nameTxt.SetEffect(EFFECT_FADE, -17);
			ResumeGui();
			while(nameTxt.GetEffect() > 0 || diskImg.GetBetaRotateEffect()) usleep(50);
			HaltGui();
			diskImg.SetImage(diskCover);
			diskImg.SetBeta(90);
			diskImg.SetBetaRotateEffect(-90, 15);
			diskImg2.SetImage(diskCover2);
			diskImg2.SetBeta(270);
			diskImg2.SetBetaRotateEffect(-90, 15);
			sizeTxt.SetEffect(EFFECT_FADE, 17);
			nameTxt.SetEffect(EFFECT_FADE, 17);
		}
		else if (changed == 4){
			diskImg.SetImage(diskCover2);
			diskImg.SetBeta(0);
			diskImg.SetBetaRotateEffect(90, 15);
			diskImg2.SetImage(diskCover);
			diskImg2.SetAngle(diskImg.GetAngle());
			diskImg2.SetBeta(180);
			diskImg2.SetBetaRotateEffect(90, 15);
			sizeTxt.SetEffect(EFFECT_FADE, -17);
			nameTxt.SetEffect(EFFECT_FADE, -17);
			ResumeGui();
			while(nameTxt.GetEffect() > 0 || diskImg.GetBetaRotateEffect()) usleep(50);
			HaltGui();
			diskImg.SetImage(diskCover);
			diskImg.SetBeta(270);
			diskImg.SetBetaRotateEffect(90, 15);
			diskImg2.SetImage(diskCover2);
			diskImg2.SetBeta(90);
			diskImg2.SetBetaRotateEffect(90, 15);
			sizeTxt.SetEffect(EFFECT_FADE, 17);
			nameTxt.SetEffect(EFFECT_FADE, 17);
		}
		else
			diskImg.SetImage(diskCover);

		WBFS_GameSize(header->id, &size);
		sizeTxt.SetTextf("%.2fGB", size); //set size text;
		nameTxt.SetText(get_title(header));

		struct Game_NUM* game_num = CFG_get_game_num(header->id);
		if (game_num) {
			playCount = game_num->count;
			faveChoice = game_num->favorite;
		} else {
			playCount = 0;
			faveChoice = 0;
		}
		playcntTxt.SetTextf("%s: %i",LANGUAGE.Plays, playCount);
 		btnFavoriteImg.SetImage(faveChoice ? &imgFavorite : &imgNotFavorite);

		nameTxt.SetPosition(0, 1);

		if(changed != 3 && changed != 4) // changed==3 or changed==4 --> only Resume the GUI
		{
			HaltGui();
			mainWindow->SetState(STATE_DISABLED);
			mainWindow->Append(&promptWindow);
			mainWindow->ChangeFocus(&promptWindow);
		}
		ResumeGui();

		changed = 0;
		while(choice == -1)
		{
			diskImg.SetSpin(btn1.GetState() == STATE_SELECTED);
			diskImg2.SetSpin(btn1.GetState() == STATE_SELECTED);
			if(shutdown == 1) //for power button
			{
				wiilight(0);
				Sys_Shutdown();
			}
			if(reset == 1) //for reset button
				Sys_Reboot();

			if(btn1.GetState() == STATE_CLICKED) { //boot
				//////////save game play count////////////////
				extern u8 favorite;
				extern u16 count;
				struct Game_NUM* game_num = CFG_get_game_num(header->id);
				if (game_num)
					{
					favorite = game_num->favorite;
					count = game_num->count;//count+=1;
					}count+=1;
				//if(isSdInserted()) {
				if(isInserted(bootDevice)) {
				if (CFG_save_game_num(header->id))
				{
					//WindowPrompt(LANGUAGE.SuccessfullySaved, 0, LANGUAGE.ok, 0,0,0);
				}
				else
				{
					//WindowPrompt(LANGUAGE.SaveFailed, 0, LANGUAGE.ok, 0,0,0);
				}
				}
				////////////end save play count//////////////

				choice = 1;
				SDCard_deInit();
			}

			else if(btn2.GetState() == STATE_CLICKED) { //back
				choice = 0;
				promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
				mainWindow->SetState(STATE_DEFAULT);
				wiilight(0);
			}

			else if(btn3.GetState() == STATE_CLICKED) { //settings
				choice = 2;
				promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			}

			else if(nameBtn.GetState() == STATE_CLICKED) { //rename
				choice = 3;
				promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
			}

			else if(btnFavorite.GetState() == STATE_CLICKED){//switch favorite
				//if(isSdInserted()) {
				if(isInserted(bootDevice)) {
					faveChoice = !faveChoice;
					btnFavoriteImg.SetImage(faveChoice ? &imgFavorite : &imgNotFavorite);
					extern u8 favorite;
					extern u8 count;
					struct Game_NUM* game_num = CFG_get_game_num(header->id);
					if (game_num) {
						favorite = game_num->favorite;
						count = game_num->count;
					}
					favorite = faveChoice;
					CFG_save_game_num(header->id);
				}
				btnFavorite.ResetState();
			}

			// this next part is long because nobody could agree on what the left/right buttons should do
			else if((btnRight.GetState() == STATE_CLICKED) && (Settings.xflip == no)){//next game
				promptWindow.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
				changed = 1;
				btnClick.Play();
				gameSelected = (gameSelected + 1) % gameCnt;
				btnRight.ResetState();
				break;
			}

			else if((btnLeft.GetState() == STATE_CLICKED) && (Settings.xflip == no)){//previous game
				promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
				changed = 2;
				btnClick.Play();
				gameSelected = (gameSelected - 1 + gameCnt) % gameCnt;
				btnLeft.ResetState();
				break;
			}

			else if((btnRight.GetState() == STATE_CLICKED) && (Settings.xflip == yes)){//previous game
				promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
				changed = 2;
				btnClick.Play();
				gameSelected = (gameSelected - 1 + gameCnt) % gameCnt;
				btnRight.ResetState();
				break;
			}

			else if((btnLeft.GetState() == STATE_CLICKED) && (Settings.xflip == yes)){//netx game
				promptWindow.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
				changed = 1;
				btnClick.Play();
				gameSelected = (gameSelected + 1) % gameCnt;
				btnLeft.ResetState();
				break;
			}

			else if((btnRight.GetState() == STATE_CLICKED) && (Settings.xflip == sysmenu)){//previous game
				promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
				changed = 2;
				btnClick.Play();
				gameSelected = (gameSelected + 1) % gameCnt;
				btnRight.ResetState();
				break;
			}

			else if((btnLeft.GetState() == STATE_CLICKED) && (Settings.xflip == sysmenu)){//netx game
				promptWindow.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
				changed = 1;
				btnClick.Play();
				gameSelected = (gameSelected - 1 + gameCnt) % gameCnt;
				btnLeft.ResetState();
				break;
			}

			else if((btnRight.GetState() == STATE_CLICKED) && (Settings.xflip == wtf)){//previous game
				promptWindow.SetEffect(EFFECT_SLIDE_RIGHT | EFFECT_SLIDE_OUT, 50);
				changed = 1;
				btnClick.Play();
				gameSelected = (gameSelected - 1 + gameCnt) % gameCnt;
				btnRight.ResetState();
				break;
			}

			else if((btnLeft.GetState() == STATE_CLICKED) && (Settings.xflip == wtf)){//netx game
				promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 50);
				changed = 2;
				btnClick.Play();
				gameSelected = (gameSelected + 1) % gameCnt;
				btnLeft.ResetState();
				break;
			}

			else if((btnRight.GetState() == STATE_CLICKED) && (Settings.xflip == disk3d)){//next game
//				diskImg.SetBetaRotateEffect(45, 90);
				changed = 3;
				btnClick.Play();
				gameSelected = (gameSelected + 1) % gameCnt;
				btnRight.ResetState();
				break;
			}

			else if((btnLeft.GetState() == STATE_CLICKED) && (Settings.xflip == disk3d)){//previous game
//				diskImg.SetBetaRotateEffect(-45, 90);
//				promptWindow.SetEffect(EFFECT_SLIDE_LEFT | EFFECT_SLIDE_OUT, 1/*50*/);
				changed = 4;
				btnClick.Play();
				gameSelected = (gameSelected - 1 + gameCnt) % gameCnt;
				btnLeft.ResetState();
				break;
			}
		}


		while(promptWindow.GetEffect() > 0) usleep(50);
		HaltGui();
		if(changed != 3 && changed != 4) // changed==3 or changed==4 --> only Halt the GUI
		{
			mainWindow->Remove(&promptWindow);
			ResumeGui();
		}
	}
	delete diskCover;
	delete diskCover2;

	return choice;
}

/****************************************************************************
 * DiscWait
 ***************************************************************************/
int
DiscWait(const char *title, const char *msg, const char *btn1Label, const char *btn2Label, int IsDeviceWait)
{
	int i = 30, ret = 0;
    u32 cover = 0;

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigB;
	trigB.SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);
	}

	GuiText titleTxt(title, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,60);
	GuiText msgTxt(msg, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-40);
	msgTxt.SetMaxWidth(430);

	GuiText btn1Txt(btn1Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn1Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn1Txt.SetWidescreen(CFG.widescreen);
	btn1Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn1(&btn1Img,&btn1Img, 1, 5, 0, 0, &trigA, &btnSoundOver, &btnClick,1);

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(40, -45);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -45);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetTrigger(&trigB);
	btn1.SetState(STATE_SELECTED);

	GuiText btn2Txt(btn2Label, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn2Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn2Txt.SetWidescreen(CFG.widescreen);
	btn2Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn2(&btn2Img,&btn2Img, 1, 4, -20, -25, &trigA, &btnSoundOver, &btnClick,1);
	btn2.SetLabel(&btn2Txt);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust buttons for widescreen
		msgTxt.SetMaxWidth(380);
		if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn2.SetPosition(-70, -80);
		btn1.SetPosition(70, -80);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -80);
	}
	}

	GuiText timerTxt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	timerTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	timerTxt.SetPosition(0,160);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);

	if(btn1Label)
	promptWindow.Append(&btn1);
	if(btn2Label)
		promptWindow.Append(&btn2);
    if(IsDeviceWait)
	promptWindow.Append(&timerTxt);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	if(IsDeviceWait) {
        while(i >= 0)
        {
            VIDEO_WaitVSync();
            timerTxt.SetTextf("%u %s", i,LANGUAGE.secondsleft);
            HaltGui();
            if(Settings.cios == ios222) {
            ret = IOS_ReloadIOS(222);
            load_ehc_module();
            } else {
            ret = IOS_ReloadIOS(249);
            }
            ResumeGui();
            sleep(1);
            ret = WBFS_Init(WBFS_DEVICE_USB);
			if(ret>=0)
            break;

            i--;
        }
	} else {
        while(!(cover & 0x2))
        {
            VIDEO_WaitVSync();
            if(btn1.GetState() == STATE_CLICKED) {
                btn1.ResetState();
                break;
            }
            ret = WDVD_GetCoverStatus(&cover);
            if (ret < 0)
                break;
        }
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return ret;
}

/****************************************************************************
 * FormatingPartition
 ***************************************************************************/
int
FormatingPartition(const char *title, partitionEntry *entry)
{
    int ret;
	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);


	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);
	}

	GuiText titleTxt(title, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,60);

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);


	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	VIDEO_WaitVSync();
	ret = WBFS_Format(entry->sector, entry->size);


	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return ret;
}


/****************************************************************************
 * SearchMissingImages
 ***************************************************************************/
void SearchMissingImages(int choice2)
{
	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);

    GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);
	}

	GuiText titleTxt(LANGUAGE.InitializingNetwork, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,60);

	char msg[20] = " ";
	GuiText msgTxt(msg, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-40);

    GuiText btn1Txt(LANGUAGE.Cancel, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn1Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn1Txt.SetWidescreen(CFG.widescreen);
	btn1Img.SetWidescreen(CFG.widescreen);
	}
	GuiButton btn1(&btn1Img,&btn1Img, 2, 4, 0, -45, &trigA, &btnSoundOver, &btnClick,1);
	btn1.SetLabel(&btn1Txt);
	btn1.SetState(STATE_SELECTED);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust buttons for widescreen
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -80);
	}

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);

	ResumeGui();

	while (!IsNetworkInit()) {

        VIDEO_WaitVSync();

        Initialize_Network();

		if (!IsNetworkInit()) {
        msgTxt.SetText(LANGUAGE.Couldnotinitializenetwork);
		}

		if(btn1.GetState() == STATE_CLICKED) {
			btn1.ResetState();
			break;
		}
    }

    if (IsNetworkInit()) {
            msgTxt.SetTextf("IP: %s", GetNetworkIP());
			cntMissFiles = 0;
			u32 i = 0;
			char filename[11];

			bool found1 = false;/////add Ids of games that are missing covers to cntMissFiles
			bool found2 = false;
			for (i = 0; i < gameCnt && cntMissFiles < 500; i++)
			{
				struct discHdr* header = &gameList[i];
				if (choice2 != 3) {

					snprintf (filename,sizeof(filename),"%c%c%c.png", header->id[0], header->id[1], header->id[2]);
					found2 = findfile(filename, Settings.covers_path);
					snprintf(filename,sizeof(filename),"%c%c%c%c%c%c.png",header->id[0], header->id[1], header->id[2],
																		header->id[3], header->id[4], header->id[5]); //full id
					found1 = findfile(filename, Settings.covers_path);
					if (!found1 && !found2) //if could not find any image
					{
						snprintf(missingFiles[cntMissFiles],11,"%s",filename);
						cntMissFiles++;
					}
				}
				else if (choice2 == 3) {
					snprintf (filename,sizeof(filename),"%c%c%c.png", header->id[0], header->id[1], header->id[2]);
					found2 = findfile(filename, Settings.disc_path);
					snprintf(filename,sizeof(filename),"%c%c%c%c%c%c.png",header->id[0], header->id[1], header->id[2],
																		header->id[3], header->id[4], header->id[5]); //full id
					found1 = findfile(filename,Settings.disc_path);
					if (!found1 && !found2)
					{
						snprintf(missingFiles[cntMissFiles],11,"%s",filename);
						cntMissFiles++;
					}
				}
			}
		}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();

	return;
}

/****************************************************************************
 * ShowProgress
 *
 * Updates the variables used by the progress window for drawing a progress
 * bar. Also resumes the progress window thread if it is suspended.
 ***************************************************************************/
void
ShowProgress (s32 done, s32 total)
{

 	static time_t start;
	static u32 expected;

	u32 d, h, m, s;

	//first time
	if (!done) {
		start    = time(0);
		expected = 300;
	}

	//Elapsed time
	d = time(0) - start;

	if (done != total) {
		//Expected time
		if (d)
			expected = (expected * 3 + d * total / done) / 4;

		//Remaining time
		d = (expected > d) ? (expected - d) : 0;
	}

	//Calculate time values
	h =  d / 3600;
	m = (d / 60) % 60;
	s =  d % 60;

    //Calculate percentage/size
	f32 percent = (done * 100.0) / total;

    prTxt.SetTextf("%0.2f", percent);

    timeTxt.SetTextf("%s %d:%02d:%02d",LANGUAGE.Timeleft,h,m,s);

    f32 gamesizedone = gamesize * done/total;

	sizeTxt.SetTextf("%0.2fGB/%0.2fGB", gamesizedone, gamesize);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){
	progressbarImg.SetTile((int)(80*done/total));}
	else {progressbarImg.SetTile((int)(100*done/total));}

}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
int
ProgressWindow(const char *title, const char *msg)
{

    wbfs_t * hdd = NULL;
    hdd = GetHddInfo();

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);}

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_outline.png", CFG.theme_path);
	GuiImageData progressbarOutline(imgPath, progressbar_outline_png);

	GuiImage progressbarOutlineImg(&progressbarOutline);
	if (Settings.wsprompt == yes){
	progressbarOutlineImg.SetWidescreen(CFG.widescreen);}
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 40);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_empty.png", CFG.theme_path);
	GuiImageData progressbarEmpty(imgPath, progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 40);
	progressbarEmptyImg.SetTile(100);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar.png", CFG.theme_path);
	GuiImageData progressbar(imgPath, progressbar_png);

	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 40);

	GuiText titleTxt(title, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,60);
	GuiText msgTxt(msg, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,120);

	GuiText prsTxt("%", 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	prsTxt.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	prsTxt.SetPosition(-188,40);

    timeTxt.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	timeTxt.SetPosition(275,-50);

    sizeTxt.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	sizeTxt.SetPosition(50, -50);

	prTxt.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	prTxt.SetPosition(200, 40);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust for widescreen
		progressbarOutlineImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
		progressbarOutlineImg.SetPosition(0, 40);
		progressbarEmptyImg.SetPosition(80,40);
		progressbarEmptyImg.SetTile(78);
		progressbarImg.SetPosition(80, 40);
		msgTxt.SetMaxWidth(380);

		timeTxt.SetPosition(250,-50);
		timeTxt.SetFontSize(22);
		sizeTxt.SetPosition(90, -50);
		sizeTxt.SetFontSize(22);
	}

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
    promptWindow.Append(&progressbarEmptyImg);
    promptWindow.Append(&progressbarImg);
    promptWindow.Append(&progressbarOutlineImg);
    promptWindow.Append(&prTxt);
	promptWindow.Append(&prsTxt);
    promptWindow.Append(&timeTxt);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();
	promptWindow.Append(&prTxt);
	promptWindow.Append(&sizeTxt);

    s32 ret;

    USBStorage_Watchdog(0);

    ret = wbfs_add_disc(hdd, __WBFS_ReadDVD, NULL, ShowProgress, ONLY_GAME_PARTITION, 0);

    USBStorage_Watchdog(1);

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	if (ret < 0) {
    return ret;
	}
	return 0;
}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
int
ProgressDownloadWindow(int choice2)
{

    int i = 0, cntNotFound = 0;

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);

    GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);}

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_outline.png", CFG.theme_path);
	GuiImageData progressbarOutline(imgPath, progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	if (Settings.wsprompt == yes){
	progressbarOutlineImg.SetWidescreen(CFG.widescreen);}
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 40);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_empty.png", CFG.theme_path);
	GuiImageData progressbarEmpty(imgPath, progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 40);
	progressbarEmptyImg.SetTile(100);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar.png", CFG.theme_path);
	GuiImageData progressbar(imgPath, progressbar_png);
	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 40);

	GuiText titleTxt(LANGUAGE.Downloadingfile, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,60);

	GuiText msgTxt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,130);

	GuiText msg2Txt(NULL, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msg2Txt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msg2Txt.SetPosition(0,100);

	prTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	prTxt.SetPosition(0, 40);

    GuiText btn1Txt(LANGUAGE.Cancel, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn1Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn1Txt.SetWidescreen(CFG.widescreen);
	btn1Img.SetWidescreen(CFG.widescreen);}
	GuiButton btn1(&btn1Img,&btn1Img, 2, 4, 0, -45, &trigA, &btnSoundOver, &btnClick,1);
	btn1.SetLabel(&btn1Txt);
	btn1.SetState(STATE_SELECTED);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust for widescreen
		progressbarOutlineImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
		progressbarOutlineImg.SetPosition(0, 40);
		progressbarEmptyImg.SetPosition(80,40);
		progressbarEmptyImg.SetTile(78);
		progressbarImg.SetPosition(80, 40);
	}

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&msg2Txt);
    promptWindow.Append(&progressbarEmptyImg);
    promptWindow.Append(&progressbarImg);
    promptWindow.Append(&progressbarOutlineImg);
    promptWindow.Append(&prTxt);
    promptWindow.Append(&btn1);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

    //check if directory exist and if not create one
    struct stat st;
    if(stat(Settings.covers_path, &st) != 0) {
        char dircovers[100];
        snprintf(dircovers,strlen(Settings.covers_path),"%s",Settings.covers_path);
        if (mkdir(dircovers, 0777) == -1) {
            if(subfoldercheck(dircovers) != 1) {
            WindowPrompt(LANGUAGE.Error,LANGUAGE.Cantcreatedirectory,LANGUAGE.ok,0,0,0);
            cntMissFiles = 0;
            }
        }
    }
    if(stat(Settings.disc_path,&st) != 0) {
        char dirdiscs[100];
        snprintf(dirdiscs,strlen(Settings.disc_path),"%s",Settings.disc_path);
        if (mkdir(dirdiscs, 0777) == -1) {
            if(subfoldercheck(dirdiscs) != 1) {
            WindowPrompt(LANGUAGE.Error,LANGUAGE.Cantcreatedirectory,LANGUAGE.ok,0,0,0);
            cntMissFiles = 0;
            }
        }
    }

	while (i < cntMissFiles)
	{

		prTxt.SetTextf("%i%%", 100*i/cntMissFiles);

		if ((Settings.wsprompt == yes) && (CFG.widescreen))
		{
			//adjust for widescreen
			progressbarImg.SetPosition(80,40);
			progressbarImg.SetTile(80*i/cntMissFiles);
		}
		else
		{
			progressbarImg.SetTile(100*i/cntMissFiles);
		}

		msgTxt.SetTextf("%i %s", cntMissFiles - i, LANGUAGE.filesleft);
		msg2Txt.SetTextf("%s", missingFiles[i]);

		//download boxart image
		char imgPath[100];
		char URLFile[100];
		if (choice2 == 2)
		{
			sprintf(URLFile,"http://www.theotherzone.com/wii/3d/176/248/%s",missingFiles[i]); // For 3D Covers
			sprintf(imgPath,"%s%s", Settings.covers_path, missingFiles[i]);
		}
		if(choice2 == 3)
		{
			sprintf(URLFile,"http://www.theotherzone.com/wii/diskart/160/160/%s",missingFiles[i]);
			sprintf(imgPath,"%s%s", Settings.disc_path, missingFiles[i]);
		}
		if(choice2 == 1)
		{
			sprintf(URLFile,"http://www.theotherzone.com/wii/resize/160/224/%s",missingFiles[i]);
			sprintf(imgPath,"%s%s", Settings.covers_path, missingFiles[i]);
		}

		struct block file = downloadfile(URLFile);//reject known bad images

		if (file.size == 36864 || file.size <= 1024 || file.size == 7386 || file.data == NULL) {
			cntNotFound++;
			i++;
		}
		else
		{
			if(file.data != NULL)
			{
				// save png to sd card
				FILE *pfile=NULL;
				if((pfile = fopen(imgPath, "wb"))!=NULL)
				{
					fwrite(file.data,1,file.size,pfile);
					fclose (pfile);
				}
				free(file.data);
			}
			i++;
		}

		if(btn1.GetState() == STATE_CLICKED)
		{
			cntNotFound = cntMissFiles-i+cntNotFound;
			break;
		}
	}

    /**Temporary redownloading 1st image because of a fucking corruption bug **/

    char URLFile[100];
    if (choice2 == 2) {
		sprintf(URLFile,"http://www.theotherzone.com/wii/3d/176/248/%s",missingFiles[0]); // For 3D Covers
		sprintf(imgPath,"%s%s", Settings.covers_path, missingFiles[0]);
    }
    if(choice2 == 3) {
		sprintf(URLFile,"http://www.theotherzone.com/wii/diskart/160/160/%s",missingFiles[0]);
		sprintf(imgPath,"%s%s", Settings.disc_path, missingFiles[0]);
    }
    if(choice2 == 1) {
		sprintf(URLFile,"http://www.theotherzone.com/wii/resize/160/224/%s",missingFiles[0]);
		sprintf(imgPath,"%s%s", Settings.covers_path, missingFiles[0]);
    }

    struct block file = downloadfile(URLFile);

    if (file.size == 36864 || file.size <= 1024 || file.size == 7386 || file.data == NULL) {
    } else {
    if(file.data != NULL)
    {
        // save png to sd card
        FILE *pfile;
        pfile = fopen(imgPath, "wb");
        fwrite(file.data,1,file.size,pfile);
        fclose (pfile);
        free(file.data);
    }
    }

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();

	if (cntNotFound != 0) {
	    return cntNotFound;
	} else {
        return 0;
	}
}

/****************************************************************************
 * ProgressWindow
 *
 * Opens a window, which displays progress to the user. Can either display a
 * progress bar showing % completion, or a throbber that only shows that an
 * action is in progress.
 ***************************************************************************/
#define BLOCKSIZE           1024

int ProgressUpdateWindow()
{
    int ret = 0, failed = 0;

	GuiWindow promptWindow(472,320);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);

    GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM, Settings.sfxvolume);
	GuiSound btnClick(button_click2_pcm, button_click2_pcm_size, SOUND_PCM, Settings.sfxvolume);

	char imgPath[100];
	snprintf(imgPath, sizeof(imgPath), "%sbutton_dialogue_box.png", CFG.theme_path);
	GuiImageData btnOutline(imgPath, button_dialogue_box_png);
	snprintf(imgPath, sizeof(imgPath), "%sdialogue_box.png", CFG.theme_path);
	GuiImageData dialogBox(imgPath, dialogue_box_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImage dialogBoxImg(&dialogBox);
	if (Settings.wsprompt == yes){
	dialogBoxImg.SetWidescreen(CFG.widescreen);}

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_outline.png", CFG.theme_path);
	GuiImageData progressbarOutline(imgPath, progressbar_outline_png);
	GuiImage progressbarOutlineImg(&progressbarOutline);
	if (Settings.wsprompt == yes){
	progressbarOutlineImg.SetWidescreen(CFG.widescreen);}
	progressbarOutlineImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarOutlineImg.SetPosition(25, 7);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar_empty.png", CFG.theme_path);
	GuiImageData progressbarEmpty(imgPath, progressbar_empty_png);
	GuiImage progressbarEmptyImg(&progressbarEmpty);
	progressbarEmptyImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarEmptyImg.SetPosition(25, 7);
	progressbarEmptyImg.SetTile(100);

	snprintf(imgPath, sizeof(imgPath), "%sprogressbar.png", CFG.theme_path);
	GuiImageData progressbar(imgPath, progressbar_png);
	progressbarImg.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	progressbarImg.SetPosition(25, 7);

    char title[50];
    sprintf(title, "%s", LANGUAGE.CheckingforUpdates);
	GuiText titleTxt(title, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,50);
    char msg[50];
    sprintf(msg, "%s", LANGUAGE.InitializingNetwork);
	GuiText msgTxt(msg, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	msgTxt.SetPosition(0,140);
	char msg2[50] = " ";
	GuiText msg2Txt(msg2, 26, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	msg2Txt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msg2Txt.SetPosition(0, 50);

	prTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	prTxt.SetPosition(0, 7);

    GuiText btn1Txt(LANGUAGE.Cancel, 22, (GXColor){THEME.prompttxt_r, THEME.prompttxt_g, THEME.prompttxt_b, 255});
	GuiImage btn1Img(&btnOutline);
	if (Settings.wsprompt == yes){
	btn1Txt.SetWidescreen(CFG.widescreen);
	btn1Img.SetWidescreen(CFG.widescreen);}
	GuiButton btn1(&btn1Img,&btn1Img, 2, 4, 0, -40, &trigA, &btnSoundOver, &btnClick,1);
	btn1.SetLabel(&btn1Txt);
	btn1.SetState(STATE_SELECTED);

	if ((Settings.wsprompt == yes) && (CFG.widescreen)){/////////////adjust for widescreen
		progressbarOutlineImg.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
		progressbarOutlineImg.SetPosition(0, 7);
		progressbarEmptyImg.SetPosition(80,7);
		progressbarEmptyImg.SetTile(78);
		progressbarImg.SetPosition(80, 7);
	}

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&msg2Txt);
    promptWindow.Append(&btn1);

    promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

    struct stat st;
    if(stat(Settings.update_path, &st) != 0) {
        char dir[100];
        snprintf(dir,strlen(Settings.update_path),"%s",Settings.update_path);
        if (mkdir(dir, 0777) == -1) {
            if(subfoldercheck(dir) != 1) {
            WindowPrompt(LANGUAGE.Error,LANGUAGE.Cantcreatedirectory,LANGUAGE.ok,0,0,0);
            ret = -1;
            failed = -1;
            }
        }
    }

    char dolpath[150];
    char dolpathsuccess[150];
    snprintf(dolpath, sizeof(dolpath), "%sbootnew.dol", Settings.update_path);
    snprintf(dolpathsuccess, sizeof(dolpathsuccess), "%sboot.dol", Settings.update_path);

	while (!IsNetworkInit()) {

        VIDEO_WaitVSync();

        Initialize_Network();

		if (IsNetworkInit()) {
		msgTxt.SetText(GetNetworkIP());
		} else {
        msgTxt.SetText(LANGUAGE.Couldnotinitializenetwork);
		}

        if(btn1.GetState() == STATE_CLICKED) {
			ret = -1;
			failed = -1;
			btn1.ResetState();
			break;
		}
	}

	if(IsNetworkInit() && ret >= 0) {

    int newrev = CheckUpdate();

    if(newrev > 0) {

        sprintf(msg, "Rev%i %s.", newrev, LANGUAGE.available);
        int choice = WindowPrompt(msg, LANGUAGE.Doyouwanttoupdate, LANGUAGE.Updatedol, LANGUAGE.Updateall, LANGUAGE.Cancel, 0);
        if(choice == 1 || choice == 2) {
            titleTxt.SetTextf("%s USB Loader GX", LANGUAGE.updating);
            msgTxt.SetPosition(0,100);
            promptWindow.Append(&progressbarEmptyImg);
            promptWindow.Append(&progressbarImg);
            promptWindow.Append(&progressbarOutlineImg);
            promptWindow.Append(&prTxt);
            msgTxt.SetTextf("%s Rev%i", LANGUAGE.Updateto, newrev);
            s32 filesize = download_request("http://www.techjawa.com/usbloadergx/boot.dol");
            if(filesize > 0) {
                FILE * pfile;
                pfile = fopen(dolpath, "wb");
                u8 blockbuffer[BLOCKSIZE] ATTRIBUTE_ALIGN(32);
                for (s32 i = 0; i < filesize; i += BLOCKSIZE) {
                    prTxt.SetTextf("%i%%", 100*i/filesize);
                    if ((Settings.wsprompt == yes) && (CFG.widescreen)) {
                        progressbarImg.SetTile(80*i/filesize);
                    } else {
                        progressbarImg.SetTile(100*i/filesize);
                    }
                    msg2Txt.SetTextf("%iKB/%iKB", i/1024, filesize/1024);

                    if(btn1.GetState() == STATE_CLICKED) {
                        fclose(pfile);
                        remove(dolpath);
                        failed = -1;
                        btn1.ResetState();
                        break;
                    }

                    u32 blksize;
                    blksize = (u32)(filesize - i);
                    if (blksize > BLOCKSIZE)
                        blksize = BLOCKSIZE;

                    ret = network_read(blockbuffer, blksize);
                    if (ret != (s32) blksize) {
                        failed = -1;
                        ret = -1;
                        fclose(pfile);
                        remove(dolpath);
                        break;
                    }
                    fwrite(blockbuffer,1,blksize, pfile);
                }
                fclose(pfile);
                if(!failed) {
                //remove old
                if(checkfile(dolpathsuccess)){
                        remove(dolpathsuccess);
                }
                //rename new to old
                rename(dolpath, dolpathsuccess);

                if(choice == 2) {
                //get the icon.png and the meta.xml
                char xmliconpath[150];
                struct block file = downloadfile("http://www.techjawa.com/usbloadergx/meta.file");
                if(file.data != NULL){
                    sprintf(xmliconpath, "%smeta.xml", Settings.update_path);
                    pfile = fopen(xmliconpath, "wb");
                    fwrite(file.data,1,file.size,pfile);
                    fclose(pfile);
                    free(file.data);
                }
                file = downloadfile("http://www.techjawa.com/usbloadergx/icon.png");
                if(file.data != NULL){
                    sprintf(xmliconpath, "%sicon.png", Settings.update_path);
                    pfile = fopen(xmliconpath, "wb");
                    fwrite(file.data,1,file.size,pfile);
                    fclose(pfile);
                    free(file.data);
                }
                }
                }
            } else {
            failed = -1;
            }
        } else {
        ret = -1;
        }

    } else {
        WindowPrompt(LANGUAGE.Nonewupdates, 0, LANGUAGE.ok, 0, 0, 0);
        ret = -1;
    }

    }

    CloseConnection();

    if(!failed && ret >= 0) {
        WindowPrompt(LANGUAGE.Successfullyupdated , LANGUAGE.Restarting, LANGUAGE.ok, 0, 0, 0);
        Sys_BackToLoader();
    }

    promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(50);

	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();

    if(failed != 0)
    return failed;

    return 1;
}

char * GetMissingFiles()
{
    return (char *) missingFiles;
}