#include "openMSXController.h"
#include "wxCatapultFrm.h"
#include "ConfigurationData.h"
#include "StatusPage.h"
#include "VideoControlPage.h"
#include "MiscControlPage.h"
#include "AudioControlPage.h"
#include "SessionPage.h"
#include "wxCatapultApp.h"
#include "ScreenShotDlg.h"
#include <cassert>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/tokenzr.h>
#include <wx/wxprec.h>
//#include <wx/version.h>

#define LAUNCHSCRIPT_MAXSIZE 100

openMSXController::openMSXController(wxWindow* target)
{
	m_openMSXID = 0;
	m_appWindow = (wxCatapultFrame*)target;
	m_openMsxRunning = false;
	m_pluggables.Clear();
	m_connectors.Clear();
	InitLaunchScript();
}

openMSXController::~openMSXController()
{
	delete[] m_launchScript;
}

bool openMSXController::HandleMessage(wxCommandEvent& event)
{
	switch (event.GetId()) {
	case MSGID_STDOUT:
		HandleStdOut(event);
		break;
	case MSGID_STDERR:
		HandleStdErr(event);
		break;
	case MSGID_PARSED:
		HandleParsedOutput(event);
		break;
	case MSGID_ENDPROCESS:
		HandleEndProcess(event);
		break;
	default:
		return false; // invalid ID
	}
	return true;
}

bool openMSXController::PreLaunch()
{
	m_parser = new CatapultXMLParser(m_appWindow);
	return true;
}

bool openMSXController::PostLaunch()
{
//	connectSocket (); // disabled since openMSX has also diabled sockets for security reasons
	char initial[] = "<openmsx-control>\n";
	WriteMessage((unsigned char*)initial, strlen(initial));
	executeLaunch();
	m_appWindow->StartTimers();
	return true;
}

bool openMSXController::connectSocket()
{
	bool bRetval = false;
	if (!m_socket) {
		// only if we don't have a socket connection
		m_socket = new wxSocketClient;
		m_socket->SetEventHandler(*m_appWindow, OPENMSX_SOCKET);
		m_socket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
		m_socket->Notify(true);

		wxIPV4address addr; // wx implemented only ipv4 so far
		addr.Hostname(wxT("localhost")); // only localhost for now
		addr.Service(9938); // openMSX port
		if (!m_socket->Connect(addr, true)) {
			// don't wait, openMSX should be available allready
			wxMessageBox(wxT("Error: openMSX not available for socket connection !"));
		} else {
			bRetval = true; // succes
		}
	}
	return bRetval;
}

void openMSXController::HandleSocketEvent(wxSocketEvent& event)
{
	switch (event.GetSocketEvent()) {
	case wxSOCKET_INPUT: {
		char buffer[1024];
		event.GetSocket()->Read(buffer, 1024);
		buffer[event.GetSocket()->LastCount() - 1] = 0;
		wxString data = wxCSConv(wxT("ISO8859-1")).cMB2WX(buffer);
		m_parser->ParseXmlInput(data, m_openMSXID);
	}
	case wxSOCKET_LOST:
		break; // not sure what to do yet
	case wxSOCKET_CONNECTION:
		break;
	default:
		break;
	}
}

void openMSXController::HandleEndProcess(wxCommandEvent& event)
{
	if (!m_openMsxRunning) return;

	wxString led[] = {
		wxT("power"), wxT("caps"),  wxT("kana"),
		wxT("pause"), wxT("turbo"), wxT("FDD")
	};
	unsigned i = 0;
	FOREACH(i, led) {
		m_appWindow->UpdateLed(led[i], wxT("off"));
	}
	m_appWindow->StopTimers();
	m_appWindow->SetStatusText(wxT("Ready"));
	m_appWindow->EnableSaveSettings(false);
	delete m_parser;
	m_appWindow->m_audioControlPage->DestroyAudioMixer();
	m_appWindow->m_audioControlPage->DisableAudioPanel();
	m_openMsxRunning = false;
	m_appWindow->m_launch_AbortButton->Enable(true);
	m_appWindow->SetControlsOnEnd();
	m_appWindow->m_launch_AbortButton->SetLabel(wxT("Start"));
	HandleNativeEndProcess();
	m_commands.clear();
}

void openMSXController::HandleStdOut(wxCommandEvent& event)
{
	auto* data = (wxString*)event.GetClientData();
	m_parser->ParseXmlInput(*data, m_openMSXID);
	delete data;
}

void openMSXController::HandleStdErr(wxCommandEvent &event)
{
	auto* data = (wxString*)event.GetClientData();
	if (*data == wxT("\n")) {
		delete data;
		return;
	}
	for (unsigned i = 0; i < m_appWindow->m_tabControl->GetPageCount(); ++i) {
		if (m_appWindow->m_tabControl->GetPageText(i) == wxT("Status Info")) {
			m_appWindow->m_tabControl->SetSelection(i);
		}
	}
	m_appWindow->m_statusPage->m_outputtext->SetDefaultStyle(wxTextAttr(
		wxColour(255, 23, 23),
		wxNullColour,
		wxFont(10, wxMODERN, wxNORMAL, wxNORMAL)));
	m_appWindow->m_statusPage->m_outputtext->AppendText(*data);
	delete data;
}

void openMSXController::HandleParsedOutput(wxCommandEvent& event)
{
	auto* data = (CatapultXMLParser::ParseResult*)event.GetClientData();
	if (data->openMSXID != m_openMSXID) {
		delete data;
		return; // another instance is allready started, ignore this message
	}
	switch (data->parseState) {
	case CatapultXMLParser::TAG_UPDATE:
		if (data->updateType == CatapultXMLParser::UPDATE_LED) {
			m_appWindow->UpdateLed (data->name, data->contents);
		}
		if (data->updateType == CatapultXMLParser::UPDATE_STATE) {
			if (data->name == wxT("cassetteplayer")) {
				m_appWindow->m_sessionPage->SetCassetteMode(data->contents);
			} else {
				m_appWindow->UpdateState(data->name, data->contents);
			}
		}
		if (data->updateType == CatapultXMLParser::UPDATE_SETTING) {
			wxString lastcmd = PeekPendingCommand();
			if ((lastcmd.Mid(0, 4) != wxT("set ")) ||
			    (lastcmd.Find(' ', true) == 3) ||
			    (lastcmd.Mid(4, lastcmd.Find(' ', true) - 4) != data->name)) {
				m_appWindow->m_videoControlPage->UpdateSetting(data->name, data->contents);
			}
		} else if (data->updateType == CatapultXMLParser::UPDATE_PLUG) {
			wxString lastcmd = PeekPendingCommand();
			if ((lastcmd.Mid(0, 5) != wxT("plug ")) ||
			    (lastcmd.Find(' ', true) == 4) ||
			    (lastcmd.Mid(5, lastcmd.Find(' ', true) - 5) != data->name)) {
				m_appWindow->m_videoControlPage->UpdateSetting(data->name, data->contents);
				executeLaunch(nullptr, m_relaunch);
			}
		} else if (data->updateType == CatapultXMLParser::UPDATE_UNPLUG) {
			wxString lastcmd = PeekPendingCommand();
			if ((lastcmd.Mid(0, 7) != wxT("unplug ")) ||
			  /*(lastcmd.Find(' ', true) == 6)) {*/
			    (lastcmd.Mid(7) != data->name)) {
				m_appWindow->m_videoControlPage->UpdateSetting(data->name, data->contents);
				executeLaunch(nullptr, m_relaunch);
			}
		} else if (data->updateType == CatapultXMLParser::UPDATE_MEDIA) {
			wxString lastcmd = PeekPendingCommand();
			bool eject = false;
			int space = lastcmd.Find(' ', false);
			if ((space != -1) && (space != (int)lastcmd.Len() - 1)) {
				eject = true;
			}
			if ((lastcmd.Mid(0, data->name.Len() + 1) != (data->name + wxT(" "))) ||
			    (!eject && (lastcmd.Mid(space + 1) != (wxT("\"") + data->contents + wxT("\"")))) ||
			    (lastcmd.Left(18) == wxT("cassetteplayer new"))) {
				m_appWindow->m_videoControlPage->UpdateSetting(data->name, data->contents);
				m_appWindow->m_sessionPage->UpdateSessionData();
			}
		}
		break;
	case CatapultXMLParser::TAG_REPLY:
		switch (data->replyState) {
		case CatapultXMLParser::REPLY_OK:
			if (PeekPendingCommandTarget() == TARGET_STARTUP) {
				HandleNormalLaunchReply(event);
			} else {
				wxString command = GetPendingCommand();
				if (command == wxT("openmsx_info fps")) {
					m_appWindow->SetFPSdisplay(data->contents);
				} else if (command == wxT("save_settings")){
					wxMessageBox(wxT("openMSX settings saved successfully!"));
				} else if (command.Left(10) == wxT("screenshot")){
					m_appWindow->m_videoControlPage->UpdateScreenshotCounter();
				}
			}
			break;
		case CatapultXMLParser::REPLY_NOK:
			if (PeekPendingCommandTarget() == TARGET_STARTUP) {
				HandleNormalLaunchReply(event);
			} else {
				wxString command = GetPendingCommand();
				if (command == wxT("plug msx-midi-in midi-in-reader")) {
					m_appWindow->m_audioControlPage->InvalidMidiInReader();
				} else if (command == wxT("plug msx-midi-out midi-out-logger")) {
					m_appWindow->m_audioControlPage->InvalidMidiOutLogger();
				} else if (command == wxT("plug pcminput wavinput")) {
					m_appWindow->m_audioControlPage->InvalidSampleFilename();
				} else if (command == wxT("plug printerport logger")) {
					m_appWindow->m_miscControlPage->InvalidPrinterLogFilename();
				} else if (command == wxT("save_settings")) {
					wxMessageBox(wxT("Error saving openMSX settings\n") + data->contents);
				} else {
					m_appWindow->m_statusPage->m_outputtext->SetDefaultStyle(wxTextAttr(
						wxColour(174, 0, 0),
						wxNullColour,
						wxFont(10, wxMODERN, wxNORMAL, wxBOLD)));
					m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("Warning: NOK received on command: "));
					m_appWindow->m_statusPage->m_outputtext->AppendText(command);
					m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("\n"));
					if (!data->contents.IsEmpty()) {
						m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("contents = "));
						m_appWindow->m_statusPage->m_outputtext->AppendText(data->contents);
						m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("\n"));
					}
				}
			}
			break;
		case CatapultXMLParser::REPLY_UNKNOWN:
			m_appWindow->m_statusPage->m_outputtext->SetDefaultStyle(wxTextAttr(
				wxColour(174, 0, 0),
				wxNullColour,
				wxFont(10, wxMODERN, wxNORMAL, wxBOLD)));
			m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("Warning: Unknown reply received!\n"));
			break;
		}
		break;
	case CatapultXMLParser::TAG_LOG:
		switch (data->logLevel) {
		case CatapultXMLParser::LOG_WARNING:
			m_appWindow->m_statusPage->m_outputtext->SetDefaultStyle(wxTextAttr(
				wxColour(174, 0, 0),
				wxNullColour,
				wxFont(10, wxMODERN, wxNORMAL, wxNORMAL)));
			break;
		case CatapultXMLParser::LOG_INFO:
		case CatapultXMLParser::LOG_UNKNOWN:
			m_appWindow->m_statusPage->m_outputtext->SetDefaultStyle(wxTextAttr(
				wxColour(0, 0, 0),
				wxNullColour,
				wxFont(10, wxMODERN, wxNORMAL, wxNORMAL)));
			break;
		}
		m_appWindow->m_statusPage->m_outputtext->AppendText(data->contents);
		m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("\n"));
		if (data->contents.Left(15) == wxT("Screen saved to")) {
			int inhibit;
			ConfigurationData::instance().GetParameter(ConfigurationData::CD_SCREENSHOTINFO, &inhibit);
			if (inhibit == 0) {
				ScreenshotDlg dlg(m_appWindow);
				dlg.ShowModal();
			}
		}
		break;
	default:
		break;
	}
	delete data;
}

bool openMSXController::WriteCommand(wxString msg, TargetType target)
{
	if (!m_openMsxRunning) return false;

	CommandEntry temp;
	temp.command = msg;
	temp.target = target;
	m_commands.push_back(temp);

	xmlChar* buffer = xmlEncodeEntitiesReentrant(nullptr, (const xmlChar*)(const char*)(wxConvUTF8.cWX2MB(msg)));
	if (!buffer) return false;

	bool result = false;
	char* commandBuffer = new char[strlen((const char*)buffer) + 25];
	strcpy(commandBuffer, "<command>");
	strcat(commandBuffer, (const char*)buffer);
	strcat(commandBuffer, "</command>\n");
	result = WriteMessage((xmlChar*)commandBuffer, strlen(commandBuffer));
	delete[] commandBuffer;

	xmlFree(buffer);
	return result;
}

wxString openMSXController::GetPendingCommand()
{
//	assert(!m_commands.empty());
	if (m_commands.empty()) {  // TODO: why is assert(!m_commands.empty()) triggered ?
		// can only happen if a reply is received without a previously sent command
		return wxString();
	}
	wxString result = m_commands.front().command;
	m_commands.pop_front();
	return result;
}

wxString openMSXController::PeekPendingCommand()
{
	return m_commands.empty() ? wxString() : m_commands.front().command;
}

enum openMSXController::TargetType openMSXController::PeekPendingCommandTarget()
{
	return m_commands.empty() ? TARGET_INTERACTIVE
	                          : m_commands.front().target;
}

bool openMSXController::StartOpenMSX(wxString cmd, bool getversion)
{
	++m_openMSXID;
	bool retval = true;
	if (getversion) {
		m_appWindow->SetStatusText(wxT("Initializing..."));
		wxString versioninfo = GetOpenMSXVersionInfo(cmd);
		retval = SetupOpenMSXParameters(versioninfo);
		m_appWindow->SetStatusText(wxT("Ready"));
	} else {
		m_appWindow->SetStatusText(wxT("Running"));
		m_appWindow->EnableSaveSettings(true);
		Launch(cmd);
	}
	return retval;
}

void openMSXController::HandleNormalLaunchReply(wxCommandEvent& event)
{
	executeLaunch(&event);
}


int openMSXController::InitConnectors(wxString dummy, wxString connectors)
{
	m_connectors.Clear();
	m_connectorclasses.Clear();
	m_connectorcontents.Clear();
	wxStringTokenizer tkz(connectors, wxT("\n"));
	while (tkz.HasMoreTokens()) {
		m_connectors.Add(tkz.GetNextToken());
	}
	return 0; // don't skip any lines in the startup script
}

void openMSXController::GetConnectors(wxArrayString& connectors)
{
	connectors.Clear();
	if (m_connectors.IsEmpty()) return;

	for (unsigned i = 0; i < m_connectors.GetCount(); ++i) {
		connectors.Add(m_connectors[i]);
	}
}

wxString openMSXController::GetConnectorClass(wxString name)
{
	if (m_connectorclasses.IsEmpty()) return wxString();

	for (unsigned i = 0; i < m_connectors.GetCount(); ++i) {
		if (m_connectors[i] == name) {
			return m_connectorclasses[i];
		}
	}
	assert(false); return wxString();
}

wxString openMSXController::GetConnectorContents(wxString name)
{
	if (m_connectorcontents.IsEmpty()) return wxString();

	for (unsigned i = 0; i < m_connectors.GetCount(); ++i) {
		if (m_connectors[i] == name) {
			return m_connectorcontents[i];
		}
	}
	assert(false); return wxString();
}

int openMSXController::InitPluggables(wxString dummy, wxString pluggables)
{
	m_pluggables.Clear();
	m_pluggabledescriptions.Clear();
	m_pluggableclasses.Clear();
	wxStringTokenizer tkz(pluggables, wxT("\n"));
	while (tkz.HasMoreTokens()) {
		m_pluggables.Add(tkz.GetNextToken());
	}
	return 0; // don't skip any lines in the startup script
}

void openMSXController::GetPluggables(wxArrayString& pluggables)
{
	pluggables.Clear();
	for (unsigned i = 0; i < m_pluggables.GetCount(); ++i) {
		pluggables.Add(m_pluggables[i]);
	}
}

void openMSXController::GetPluggableDescriptions(wxArrayString& descriptions)
{
	descriptions.Clear();
	for (unsigned i = 0; i < m_pluggabledescriptions.GetCount(); ++i) {
		descriptions.Add(m_pluggabledescriptions[i]);
	}
}

void openMSXController::GetPluggableClasses(wxArrayString& classes)
{
	classes.Clear();
	for (unsigned i = 0; i < m_pluggableclasses.GetCount(); ++i) {
		classes.Add(m_pluggableclasses[i]);
	}
}

bool openMSXController::SetupOpenMSXParameters(wxString version)
{
	long ver = -1;
	long majver;
	long minver;
	long subver;
	wxString temp = version;
	if (version.Mid(0, 8) == wxT("openMSX ")) {
		int pos = version.Find('.');
		if (pos != -1) {
			temp.Mid(8, pos - 8).ToLong(&majver);
			temp = temp.Mid(pos + 1);
			pos = temp.Find('.');
			if (pos != -1) {
				temp.Mid(0, pos).ToLong(&minver);
				temp.Mid(pos + 1).ToLong(&subver);
				ver = (((majver * 100) + minver) * 100) + subver;
			}
		}
	}
	// printf ("Detected openMSX version: %d\n", ver);
	if (ver == -1) {
		wxMessageBox(
			wxT("Unable to determine openMSX version!\nPlease upgrade to 0.6.2 or higher.\n(Or contact the authors.)"),
			wxT("Error"));
		return false;
	}
	if (ver < 602) {
		wxMessageBox(
			wxT("The openMSX version you are using is too old!\nPlease upgrade to 0.6.2 or higher."),
			wxT("Error"));
		return false;
	}
	m_appWindow->m_miscControlPage->FillInitialJoystickPortValues();
	m_appWindow->m_launch_AbortButton->Enable(true);
	return true;
}

void openMSXController::InitLaunchScript()
{
	m_launchScriptSize = 0;
	m_launchScript = new LaunchInstructionType[LAUNCHSCRIPT_MAXSIZE];
	// Use __catapult_update to support both old and new openmsx versions
	AddLaunchInstruction(wxT("proc __catapult_update { args } { if {[info command openmsx_update] != \"\"} { eval \"openmsx_update $args\" } else { eval \"update $args\" } }"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("__catapult_update enable setting"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("__catapult_update enable led"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("set power on"), wxT("e"), wxT("power"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("unset renderer"), wxT("e"), wxT(""), nullptr, true);
	AddLaunchInstruction(wxT("@execute"), wxT(""), wxT(""), &openMSXController::EnableMainWindow, false);
	AddLaunchInstruction(wxT("^info renderer"), wxT(""), wxT("RendererSelector"), &openMSXController::FillComboBox, true);
	AddLaunchInstruction(wxT("^info scale_algorithm"), wxT(""), wxT("ScalerAlgoSelector"), &openMSXController::FillComboBox, true);
	AddLaunchInstruction(wxT("lindex [openmsx_info setting scale_factor] 2"), wxT(""), wxT("ScalerFactorSelector"), &openMSXController::FillRangeComboBox, true);
	AddLaunchInstruction(wxT("^info accuracy"), wxT(""), wxT("AccuracySelector"), &openMSXController::FillComboBox, false);
	AddLaunchInstruction(wxT("__catapult_update enable media"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("info exist frontswitch"), wxT(""), wxT("#"), &openMSXController::EnableFirmware, false);
	AddLaunchInstruction(wxT("info exist firmwareswitch"), wxT(""), wxT("#"), &openMSXController::EnableFirmware, false);
	AddLaunchInstruction(wxT("set renderer"), wxT(""), wxT("renderer"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set scale_algorithm"), wxT(""), wxT("scale_algorithm"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set scale_factor"), wxT(""), wxT("scale_factor"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set accuracy"), wxT(""), wxT("accuracy"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set deinterlace"), wxT(""), wxT("deinterlace"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set limitsprites"), wxT(""), wxT("limitsprites"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set fullscreen"), wxT(""), wxT("fullscreen"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set blur"), wxT(""), wxT("blur"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set glow"), wxT(""), wxT("glow"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set gamma"), wxT(""), wxT("gamma"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set scanline"), wxT("0"), wxT("scanline"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("info exist renshaturbo"), wxT("1"), wxT("#"), &openMSXController::EnableRenShaTurbo, false);
	AddLaunchInstruction(wxT("set renshaturbo"), wxT("0"), wxT("renshaturbo"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set save_settings_on_exit"), wxT(""), wxT("save_settings_on_exit"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set printerlogfilename"), wxT(""), wxT("printerlogfilename"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("@execute"), wxT(""), wxT(""), &openMSXController::SetSliderDefaults, false);
	AddLaunchInstruction(wxT("set speed"), wxT(""), wxT("speed"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set maxframeskip"), wxT(""), wxT("maxframeskip"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set minframeskip"), wxT(""), wxT("minframeskip"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set throttle"), wxT(""), wxT("throttle"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set cmdtiming"), wxT(""), wxT("cmdtiming"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("!info pluggable"), wxT("13"), wxT(""), &openMSXController::InitPluggables, true);
	AddLaunchInstruction(wxT("!info_nostore pluggable *"), wxT(""), wxT("*"), &openMSXController::AddPluggableDescription, true);
	AddLaunchInstruction(wxT("!info_nostore connectionclass *"), wxT(""), wxT("*"), &openMSXController::AddPluggableClass, false);
	AddLaunchInstruction(wxT("!info connector"), wxT("10"), wxT(""), &openMSXController::InitConnectors, true);
	AddLaunchInstruction(wxT("!info_nostore connectionclass *"), wxT(""), wxT("*"), &openMSXController::AddConnectorClass, false);
	AddLaunchInstruction(wxT("plug *"), wxT(""), wxT("*"), &openMSXController::AddConnectorContents, true);
	AddLaunchInstruction(wxT("@checkfor msx-midi-in"), wxT("1"), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("set midi-in-readfilename"), wxT(""), wxT("midi-in-readfilename"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("@checkfor msx-midi-out"), wxT("1"), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("set midi-out-logfilename"), wxT(""), wxT("midi-out-logfilename"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("@checkfor pcminput"), wxT("1"), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("set audio-inputfilename"), wxT(""), wxT("audio-inputfilename"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("@execute"), wxT(""), wxT(""), &openMSXController::InitConnectorPanel, false);
	m_relaunch = m_launchScriptSize; // !!HACK!!
	AddLaunchInstruction(wxT("@execute"), wxT(""), wxT(""), &openMSXController::InitAudioConnectorPanel, false);
//	AddLaunchInstruction(wxT("#info romtype"), wxT(""), wxT(""), &openMSXController::InitRomTypes, true);
//	AddLaunchInstruction(wxT("#info_nostore romtype *"), wxT(""), wxT("*"), &openMSXController::SetRomDescription, true);
	AddLaunchInstruction(wxT("!info sounddevice"), wxT("5"), wxT(""), &openMSXController::InitSoundDevices, true);
	AddLaunchInstruction(wxT("!info_nostore sounddevice *"), wxT(""), wxT("*"), &openMSXController::SetChannelType, true);
	AddLaunchInstruction(wxT("set master_volume"), wxT(""), wxT("master_volume"), &openMSXController::UpdateSetting, false);
	AddLaunchInstruction(wxT("set *_volume"), wxT(""), wxT("*_volume"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set *_balance"), wxT(""), wxT("*_balance"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("set mute"), wxT(""), wxT("mute"), &openMSXController::UpdateSetting, true);
	AddLaunchInstruction(wxT("plug cassetteport"), wxT(""), wxT("cassetteport"), &openMSXController::EnableCassettePort, false);
	AddLaunchInstruction(wxT("join [cassetteplayer] \\n"), wxT(""), wxT(""), &openMSXController::SetCassetteMode, false);
	AddLaunchInstruction(wxT("__catapult_update enable plug"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("__catapult_update enable unplug"), wxT(""), wxT(""), nullptr, false);
	AddLaunchInstruction(wxT("__catapult_update enable status"), wxT(""), wxT(""), nullptr, false);
}

void openMSXController::AddLaunchInstruction(
	wxString cmd, wxString action, wxString parameter,
	int (openMSXController::*pfunction)(wxString, wxString),
	bool showError)
{
	if (m_launchScriptSize >= LAUNCHSCRIPT_MAXSIZE) {
		wxMessageBox(wxT("Not enough space to store the Launchscript!\nPlease contact the authors."),
		             wxT("Internal Catapult Error"));
		return;
	}
	// Add the instruction, parameters and other flags to the launch script
	m_launchScript[m_launchScriptSize].command = cmd;
	m_launchScript[m_launchScriptSize].scriptActions = action;
	m_launchScript[m_launchScriptSize].parameter = parameter;
	m_launchScript[m_launchScriptSize].p_okfunction = pfunction;
	m_launchScript[m_launchScriptSize].showError = showError;
	++m_launchScriptSize;
}

void openMSXController::executeLaunch(wxCommandEvent* event, int startLine)
{
	static bool wait;
	static int sendStep = 0;
	static int recvStep = 0;
	static int sendLoop = -1;
	static int recvLoop = -1;
	static wxString lastdata;

	if (event) {
		auto* data = (CatapultXMLParser::ParseResult*)event->GetClientData();
		// handle received command
		wxString command = GetPendingCommand();
		wxString instruction  = m_launchScript[recvStep].command;
		while (instruction.Mid(0, 1) == wxT("@")) {
			++recvStep;
			instruction = m_launchScript[recvStep].command;
		}
		if ((recvLoop == -1) && (instruction.Find(wxT("*")) != -1)) {
			recvLoop = 0;
		}
		wxArrayString tokens;
		tokenize(instruction, wxT(" "), tokens);
		wxString cmd = translate(tokens, recvLoop, lastdata);
		if (command == cmd) {
			if (tokens[0] == wxT("#info")) {
				lastdata = data->contents;
			}
			if (tokens[0] == wxT("^info")) {
				lastdata = data->contents;
			}
			if (tokens[0] == wxT("!info")) {
				lastdata = data->contents;
			}
			HandleLaunchReply(cmd, event, m_launchScript[recvStep], &sendStep, recvLoop, lastdata);
			if ((data->replyState == CatapultXMLParser::REPLY_NOK) ||
			    ((cmd.Mid(0, 10) == wxT("info exist")) && (data->contents == wxT("0")))) {
				long displace;
				m_launchScript[recvStep].scriptActions.ToLong(&displace);
				recvStep += displace;
			}
			if (cmd == wxT("!done")) {
				recvStep = -2; // it is gonna be increased in a moment
				recvLoop = -1;
			}

			if (recvLoop != -1) {
				wxArrayString lastvalues;
				if (recvLoop < int(tokenize(lastdata, wxT("\n"), lastvalues) - 1)) {
					++recvLoop;
				} else {
					recvLoop = -1;
					++recvStep;
				}
			} else {
				++recvStep;
			}
		}
	} else {
		// init chain of events
		sendStep = startLine;
		recvStep = startLine;
		sendLoop = -1;
		recvLoop = -1;
	}
	if (recvStep >= m_launchScriptSize) {
		recvStep = 0;
		FinishLaunch();
		return;
	}
	if (wait) {
		while (m_launchScript[recvStep].command.Mid(0, 1) == wxT("@")) {
			++recvStep;
		}
		if (recvStep >= sendStep) {
			wait = false;
		}
	}

	while ((!wait) && (sendStep < m_launchScriptSize)) {
		wxString instruction = m_launchScript[sendStep].command;
		if ((sendLoop == -1) && (instruction.Find(wxT("*")) != -1)) {
			sendLoop = 0;
		}
		wxArrayString tokens;
		tokenize(instruction, wxT(" "), tokens);
		wxString cmd = translate(tokens, sendLoop, lastdata);
		if (cmd.Mid(0, 1) == wxT("@")) {
			wxString result = wxT("0");
			if (tokens[0] == wxT("@checkfor")) {
				bool contains = lastdata.Contains(tokens[1]);
				if (contains) {
					result = wxT("1");
				}
			} else {
				// @execute
				result = wxT("1");
			}
			if (result == wxT("0")) {
				while (m_launchScript[recvStep].command.Mid(0, 1) == wxT("@")) {
					++recvStep;
				}
				long displace;
				m_launchScript[sendStep].scriptActions.ToLong(&displace);
				recvStep += displace;
			}
			HandleLaunchReply(tokens[0] + result, nullptr, m_launchScript[sendStep], &sendStep, -1, wxT(""));
		} else {
			WriteCommand(cmd, TARGET_STARTUP);
		}
		wxString action = m_launchScript[sendStep].scriptActions;

		if (sendLoop != -1) {
			wxArrayString lastvalues;
			if (sendLoop < int(tokenize(lastdata, wxT("\n"), lastvalues) - 1)) {
				++sendLoop;
			} else {
				sendLoop = -1;
				++sendStep;
				wait = true;
			}
		} else {
			++sendStep;
		}

		if (!action.IsEmpty()) {
			if (recvStep < sendStep) {
				wait = true;
			}
		}
	}
}

void openMSXController::FinishLaunch()
{
	m_appWindow->m_sessionPage->AutoPlugCassette();
	m_appWindow->SetControlsOnLaunch();
	m_appWindow->m_sessionPage->SetCassetteControl();
}

size_t openMSXController::tokenize(
	const wxString& text, const wxString& seperator, wxArrayString& result)
{
	wxStringTokenizer tkz(text, seperator);
	while (tkz.HasMoreTokens()) {
		result.Add(tkz.GetNextToken());
	}
	return result.GetCount();
}

wxString openMSXController::translate(wxArrayString tokens, int loop, wxString lastdata)
{
	unsigned token;
	for (token = 0; token < tokens.GetCount(); ++token) {
		if (tokens[token].Find(wxT("*")) != -1) {
			if (loop != -1) {
				wxArrayString lastvalues;
				tokenize(lastdata, wxT("\n"), lastvalues);
				if (loop < (int)lastvalues.GetCount()) {
					tokens[token].Replace(wxT("*"), lastvalues[loop], true);
					tokens[token].Replace(wxT(" "), wxT("\\ "), true);
				}
			}
		}
	}
	token = 0;
	while (token < tokens.GetCount()) {
		switch (tokens[token][0]) {
		case '#':
			if (tokens[token].Mid(0, 5) == wxT("#info")) {
				wxString parameter;
				while (token < (tokens.GetCount() - 1)) {
					parameter += tokens[token + 1];
					parameter += wxT(" ");
					tokens.RemoveAt(token + 1);
				}
				parameter.Trim(true);
				tokens[token] = wxT("join [openmsx_info ") + parameter + wxT("] \\n");
			} else {
				assert(false); // invalid command
			}
			break;
		case '!':
			if (tokens[token].Mid(0, 5) == wxT("!info")) {
				wxString parameter;
				while (token < (tokens.GetCount() - 1)) {
					parameter += tokens[token + 1];
					parameter += wxT(" ");
					tokens.RemoveAt(token + 1);
				}
				parameter.Trim(true);
				tokens[token] = wxT("join [machine_info ") + parameter + wxT("] \\n");
			} else {
				assert(false); // invalid command
			}
			break;
		case '^':
			if (tokens[token].Mid(0, 5) == wxT("^info")) {
				wxString parameter;
				while (token < (tokens.GetCount() - 1)) {
					parameter += tokens[token + 1];
					parameter += wxT(" ");
					tokens.RemoveAt(token + 1);
				}
				parameter.Trim(true);
				tokens[token] = wxT("join [lindex [openmsx_info setting ") + parameter + wxT("] 2] \\n");
			} else {
				assert(false); // invalid command
			}
			break;
		default:
			break;
		}
		++token;
	}
	wxString result;
	for (token = 0; token < tokens.GetCount(); ++token) {
		result += tokens[token];
		result += wxT(" ");
	}
	result.Trim(true);
	return result;
}

void openMSXController::HandleLaunchReply(
	wxString cmd, wxCommandEvent* event,
	LaunchInstructionType instruction, int* sendStep, int loopcount,
	wxString datalist)
{
	CatapultXMLParser::ParseResult* data = nullptr;
	if (event) {
		data = (CatapultXMLParser::ParseResult*)event->GetClientData();
	}
	bool ok = false;
	if (cmd.Mid(0, 1) == wxT("@")) {
		if (cmd.Mid(cmd.Len() - 1) == wxT("1")) {
			ok = true;
		}
	} else {
		if (cmd.Mid(0, 10) == wxT("info exist")) {
			if (data->contents == wxT("1")) {
				ok = true;
			}
		} else {
			if (data->replyState == CatapultXMLParser::REPLY_OK) {
				ok = true;
			}
		}
	}
	wxString actions = instruction.scriptActions;

	if (ok) {
		if (instruction.p_okfunction != nullptr) {
			wxString parameter = instruction.parameter;
			wxString contents;
			if (loopcount > -1) {
				wxArrayString temp;
				tokenize(datalist, wxT("\n"), temp);
				parameter.Replace(wxT("*"), temp[loopcount], true);
				parameter.Replace(wxT(" "), wxT("\\ "), true);
			}
			if (parameter == wxT("#")) {
				parameter = cmd;
			}
			if (event) {
				contents = data->contents;
			}
			int result = (*this.*(instruction.p_okfunction))(parameter, contents);
			if (result > 0) {
				*sendStep += result;
			}
		}
	} else {
		if (instruction.showError) {
			m_appWindow->m_statusPage->m_outputtext->AppendText(wxT("Error received on command: ") + cmd + wxT("\n"));
		}
		if (!actions.IsEmpty()) {
			if (actions == wxT("e")) {
				*sendStep = m_launchScriptSize;
			} else {
				long displace;
				actions.ToLong(&displace);
				*sendStep += displace;
			}
		}
	}
}

int openMSXController::UpdateSetting(wxString setting, wxString data)
{
	m_appWindow->m_videoControlPage->UpdateSetting(setting, data); // Just use any instance of CatapultPage
	return 0; // don't skip any lines in the startup script
}

int openMSXController::FillComboBox(wxString control, wxString data)
{
	auto* box = (wxComboBox*)wxWindow::FindWindowByName(control);
	wxStringTokenizer tkz(data, wxT("\n"));
	while (tkz.HasMoreTokens()) {
		box->Append(tkz.GetNextToken());
	}
	return 0; // don't skip any lines in the startup script
}

int openMSXController::FillRangeComboBox(wxString setting, wxString data)
{
	long min;
	long max;
	wxString range;
	int pos = data.Find(' ');
	if (pos != -1) {
		data.Left(pos).ToLong(&min);
		data.Mid(pos + 1).ToLong(&max);
		for (long index = min; index <= max; ++index) {
			range << index << wxT("\n");
		}
	}
	FillComboBox(setting, range);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::EnableFirmware(wxString cmd, wxString data)
{
	if ((data != wxT("0")) || (cmd.Mid(0, 4) == wxT("set "))) {
		if (cmd.Find(wxT("frontswitch")) != -1) {
			m_appWindow->m_miscControlPage->EnableFirmware(wxT("frontswitch"));
		} else {
			m_appWindow->m_miscControlPage->EnableFirmware(wxT("firmwareswitch"));
		}
	}
	return 0; // don't skip any lines in the startup script
}

int openMSXController::EnableRenShaTurbo(wxString cmd, wxString data)
{
	if ((data != wxT("0")) || (cmd.Mid(0, 4) == wxT("set "))) {
		m_appWindow->m_miscControlPage->EnableRenShaTurbo();
	}
	return 0; // don't skip any lines in the startup script
}

int openMSXController::EnableMainWindow(wxString dummy1, wxString dummy2)
{
	m_appWindow->EnableMainWindow();
	return 0; // don't skip any lines in the startup script
}

int openMSXController::InitRomTypes(wxString dummy, wxString data)
{
	wxArrayString types;
	tokenize(data, wxT("\n"), types);
	for (unsigned i = 0; i < types.GetCount(); ++i) {
		m_appWindow->m_sessionPage->AddRomType(types[i]);
	}
	return 0;
}

int openMSXController::SetRomDescription(wxString name, wxString data)
{
	m_appWindow->m_sessionPage->SetRomTypeFullName(name, data);
	return 0;
}

int openMSXController::InitSoundDevices(wxString dummy, wxString data)
{
	wxArrayString channels;
	tokenize(data, wxT("\n"), channels);
	if (channels.GetCount() != (m_appWindow->m_audioControlPage->GetNumberOfAudioChannels() - 1)) {
		m_appWindow->m_audioControlPage->DestroyAudioMixer();
		m_appWindow->m_audioControlPage->InitAudioChannels(channels);
		return 0;
	}
	return 5; // skip 5 instructions TODO: improve this
}

int openMSXController::SetChannelType(wxString name, wxString data)
{
	int maxchannels = m_appWindow->m_audioControlPage->GetNumberOfAudioChannels();
	int index = 0;
	while (index < maxchannels) {
		if (m_appWindow->m_audioControlPage->GetAudioChannelName(index) == name) break;
		++index;
	}
	assert(index < maxchannels);

	m_appWindow->m_audioControlPage->AddChannelType(index, data);
	if (index == (maxchannels - 1)) {
		m_appWindow->m_audioControlPage->SetupAudioMixer();
	}
	return 0; // don't skip any lines in the startup script
}

int openMSXController::AddPluggableDescription(wxString name, wxString data)
{
	m_pluggabledescriptions.Add(data);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::AddPluggableClass(wxString name, wxString data)
{
	m_pluggableclasses.Add(data);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::AddConnectorClass(wxString name, wxString data)
{
	m_connectorclasses.Add(data);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::AddConnectorContents(wxString name, wxString data)
{
	m_connectorcontents.Add(data);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::SetSliderDefaults(wxString dummy1, wxString dummy2)
{
	m_appWindow->m_videoControlPage->SetSliderDefaults();
	return 0; // don't skip any lines in the startup script
}

int openMSXController::InitAudioConnectorPanel(wxString dummy1, wxString dummy2)
{
	m_appWindow->m_audioControlPage->InitAudioIO();
	return 0; // don't skip any lines in the startup script
}

int openMSXController::InitConnectorPanel(wxString dummy1, wxString dummy2)
{
	m_appWindow->m_miscControlPage->InitConnectorPanel();
	return 0; // don't skip any lines in the startup script
}

int openMSXController::EnableCassettePort(wxString dummy, wxString data)
{
	m_appWindow->m_sessionPage->EnableCassettePort(data);
	return 0; // don't skip any lines in the startup script
}

int openMSXController::SetCassetteMode(wxString dummy, wxString data)
{
	wxArrayString arrayData;
	int args = tokenize(data, wxT("\n"), arrayData);
	m_appWindow->m_sessionPage->SetCassetteMode(arrayData[args - 1]);
	return 0;
}

void openMSXController::UpdateMixer()
{
	executeLaunch(nullptr, m_relaunch);
}
