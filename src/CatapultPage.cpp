// $Id: CatapultPage.cpp,v 1.16 2004/04/27 17:01:20 h_oudejans Exp $
// CatapultPage.cpp: implementation of the CatapultPage class.
//
//////////////////////////////////////////////////////////////////////
#include "wx/wxprec.h"
#include "wx/xrc/xmlres.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "CatapultPage.h"
#include <wx/notebook.h>
#include "AudioControlPage.h"
#include "VideoControlPage.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CatapultPage::CatapultPage(wxWindow * parent)
{
	m_parent = parent;
	InitSettingsTable();
}

CatapultPage::~CatapultPage()
{
	delete [] m_settingTable;
}

wxString CatapultPage::ConvertPath(wxString path, bool ConvertSlash)
{
	path.Prepend(_("\""));
	path.Append(_("\""));
	if (ConvertSlash)
		path.Replace(_("\\"),_("/"),true);
	return path;
}

void CatapultPage::OnClickCombo (wxCommandEvent &event)
{
	wxComboBox * box = (wxComboBox *)event.GetEventObject();
	wxString sel = box->GetString(box->GetSelection());
	wxString cursel = box->GetValue();
	if (sel != cursel){
		box->SetValue(sel);
	}
}

void CatapultPage::InitSettingsTable ()
{
	m_settingTableSize = 0;
	m_settingTable = new SettingTableElementType [SETTINGTABLE_MAXSIZE];
	AddSetting("renderer","RendererSelector",&CatapultPage::UpdateComboSetting,false);
	AddSetting("scaler","ScalerSelector",&CatapultPage::UpdateComboSetting,false);
	AddSetting("accuracy","AccuracySelector",&CatapultPage::UpdateComboSetting,false);
	AddSetting("deinterlace","DeInterlaceButton",&CatapultPage::UpdateToggleSetting,true);
	AddSetting("limitsprites","LimitSpriteButton",&CatapultPage::UpdateToggleSetting,true);
	AddSetting("blur","BlurIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("glow","GlowIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("gamma","GammaIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("scanline","ScanlineIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("speed","SpeedIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("frameskip","FrameSkipIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("maxframeskip","FrameSkipIndicator",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("throttle","ThrottleButton",&CatapultPage::UpdateToggleSetting,true);
	AddSetting("cmdtiming","CmdTimingButton",&CatapultPage::UpdateToggleSetting,true);
	AddSetting("power","PowerButton",&CatapultPage::UpdateToggleSetting,false);
	AddSetting("pause","PauseButton",&CatapultPage::UpdateToggleSetting,false);
	AddSetting("frontswitch","FirmwareButton",&CatapultPage::UpdateToggleSetting,false);
	AddSetting("mute","MuteButton",&CatapultPage::UpdateToggleSetting,false);
	AddSetting("midi-in-readfilename","MidiInFileInput",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("midi-out-logfilename","MidiOutFileInput",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("audio-inputfilename","SampleFileInput",&CatapultPage::UpdateIndicatorSetting,false);
	AddSetting("*_volume","volume",&CatapultPage::UpdateAudioSetting,false);
	AddSetting("*_mode","mode",&CatapultPage::UpdateAudioSetting,false);
	AddSetting("msx-midi-in","MidiInSelector",&CatapultPage::UpdateMidiPlug,false);
	AddSetting("msx-midi-out","MidiOutSelector",&CatapultPage::UpdateMidiPlug,false);
	AddSetting("pcminput","SampleInSelector",&CatapultPage::UpdatePluggable,false);
	AddSetting("joyporta","Joyport1Selector",&CatapultPage::UpdatePluggable,false);
	AddSetting("joyportb","Joyport2Selector",&CatapultPage::UpdatePluggable,false);
	AddSetting("diska","DiskAContents",&CatapultPage::UpdateComboSetting,false);
	AddSetting("diskb","DiskBContents",&CatapultPage::UpdateComboSetting,false);
	AddSetting("tape1","Tape1Contents",&CatapultPage::UpdateComboSetting,false);
	AddSetting("tape2","Tape2Contents",&CatapultPage::UpdateComboSetting,false);
	AddSetting("fullscreen","FullScreenButton",&CatapultPage::UpdateToggleSetting,true);
}

void CatapultPage::AddSetting (wxString setting, wxString controlname,
	bool (CatapultPage::*pfunction)(wxString,wxString,wxString,bool),
	bool convert)
{	
	if (m_settingTableSize >= SETTINGTABLE_MAXSIZE){
		wxMessageBox ("Not enough space to store the Settingtable","Internal Catapult Error");
		return;
	}
	m_settingTable[m_settingTableSize].setting = setting;
	m_settingTable[m_settingTableSize].controlname = controlname;
	m_settingTable[m_settingTableSize].pfunction = pfunction;
	m_settingTable[m_settingTableSize].convert = convert;
	m_settingTableSize ++;
}

void CatapultPage::UpdateSetting(wxString name, wxString data)
{
	int index = 0;
	bool found = false;
	while ((!found) && (index < SETTINGTABLE_MAXSIZE)){

		if (name.Matches(m_settingTable[index].setting.c_str())){
			found = true;
			if (m_settingTable[index].pfunction != NULL){
				(*this.*(m_settingTable[index].pfunction))(
					name, data, m_settingTable[index].controlname,
					m_settingTable[index].convert);
			}
		}
	index ++;
	}
}

bool CatapultPage::UpdateToggleSetting(wxString setting, wxString data, wxString control, bool convert)
{
	wxString ButtonText;
#ifdef __WINDOWS__
	wxNotebook * notebook = (wxNotebook *) m_parent;
	VideoControlPage * videopage = NULL;
	for (int i=0;i<notebook->GetPageCount();i++){
		if (notebook->GetPageText(i) == _("Video Controls")){
			videopage = (VideoControlPage *)notebook->GetPage(i);
		}
	}
	if ((setting == _("fullscreen")) && (data == _("off"))){
		videopage->RestoreNormalScreen();
	}
#endif
	
	wxToggleButton * button = (wxToggleButton *)m_parent->FindWindow(control);
	if (button != NULL){
		if ((data == "on") || (data == "broken")){
			button->SetValue(true);
		}
		else{
			button->SetValue(false);
		}
		if (convert){
			ButtonText = data.Mid(0,1).Upper() + data.Mid(1).Lower();
			button->SetLabel(ButtonText);
		}
		return true;
	}
	return false;
}

bool CatapultPage::UpdateComboSetting(wxString setting, wxString data, wxString control, bool convert)
{
	wxString valuetext = data;
	if (convert){
		valuetext = data.Mid(0,1).Upper() + data.Mid(1).Lower();
	}
	wxComboBox * box = (wxComboBox *)m_parent->FindWindow(control);
	if (box != NULL){
		if (box->GetWindowStyle() & wxCB_READONLY){
			box->SetSelection(box->FindString(valuetext));
		}
		else{
			box->SetValue(valuetext);
		}
		return true;
	}
	return false;
}

bool CatapultPage::UpdateIndicatorSetting(wxString setting, wxString data, wxString control, bool dummy)
{
	wxTextCtrl * indicator = (wxTextCtrl *)m_parent->FindWindow(control);
	if (indicator != NULL){
		if (indicator->GetValue() != data)
			indicator->SetValue(data);
		return true;
	}
	return false;
}

bool CatapultPage::UpdateAudioSetting (wxString setting, wxString data, wxString selection, bool dummy)
{
	int i;
	wxString slidertext;
	wxNotebook * notebook = (wxNotebook *) m_parent;
	AudioControlPage * audiopage = NULL;
	for (i=0;i<notebook->GetPageCount();i++){
		if (notebook->GetPageText(i) == _("Audio Controls")){
			audiopage = (AudioControlPage *)notebook->GetPage(i);
		}
	}
	for (i=0;i<audiopage->GetNumberOfAudioChannels();i++){
		if ((audiopage->GetAudioChannelName(i) + _("_") + selection) == setting){
			if (selection == _("volume")){
				audiopage->SetChannelVolume(i,data);
			}
			else{
				audiopage->SetChannelMode(i,data);
			}
			return true;
		}
	}
	return false;
}

bool CatapultPage::UpdateMidiPlug (wxString connector, wxString data, wxString control, bool dummy)
{
	wxNotebook * notebook = (wxNotebook *) m_parent;
	AudioControlPage * audiopage = NULL;
	for (int i=0;i<notebook->GetPageCount();i++){
		if (notebook->GetPageText(i) == _("Audio Controls")){
			audiopage = (AudioControlPage *)notebook->GetPage(i);
		}
	}
	audiopage->UpdateMidiPlug (control, data);
	return true;
}

bool CatapultPage::UpdatePluggable (wxString connector, wxString data, wxString control, bool dummy)
{
	wxString valuetext = data;
	if (data == _("")){
		valuetext = _("--empty--");
	}
	wxComboBox * box = (wxComboBox *)m_parent->FindWindow(control);
	if (box != NULL){
		box->SetValue(valuetext);
	}
	return true;
}
