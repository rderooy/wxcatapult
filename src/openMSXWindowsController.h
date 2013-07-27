#ifndef OPENMSXWINDOWSCONTROLLER_H
#define OPENMSXWINDOWSCONTROLLER_H

#include "openMSXController.h"
#include <windows.h>

class PipeConnectThread;
class openMSXWindowsController : public openMSXController
{
public:
	void RaiseOpenMSX();
	void RestoreOpenMSX();
	void HandleEndProcess (wxCommandEvent &event);
	bool WriteMessage (xmlChar * msg,size_t length);
	void HandlePipeCreated();
	virtual bool Launch (wxString cmdLine);
	virtual bool HandleMessage (wxCommandEvent & event);
	virtual wxString GetOpenMSXVersionInfo(wxString openmsxCmd);
	virtual void HandleNativeEndProcess () {};
	openMSXWindowsController(wxWindow * target);
	virtual ~openMSXWindowsController();

private:
	struct FindOpenmsxInfo {
		LPPROCESS_INFORMATION ProcessInfo;
		HWND hWndFound;
	};

	HWND FindOpenMSXWindow();
	static BOOL CALLBACK EnumWindowCallBack(HWND hwnd, LPARAM lParam);
	void CloseHandles (bool useNamedPipes, HANDLE hThread, HANDLE hInputRead,
			HANDLE hOutputWrite, HANDLE hErrorWrite);
	void ShowError (wxString msg);
	bool CreatePipes (bool useNamedPipes,HANDLE * input, HANDLE * output, HANDLE * error,
			HANDLE * outputWrite, HANDLE * errorWrite);
	wxString CreateControlParameter (bool useNamedPipes);
	bool DetermenNamedPipeUsage ();

	HANDLE m_outputHandle;
	HANDLE m_namedPipeHandle;
	HWND m_catapultWindow;
	PROCESS_INFORMATION m_openmsxProcInfo;
	bool m_pipeActive;
	unsigned long m_launchCounter;
	PipeConnectThread * m_connectThread;
};

#endif
