// $Id: openMSXLinuxController.cpp,v 1.7 2004/04/12 19:03:58 m9710797 Exp $
// openMSXLinuxController.cpp: implementation of the openMSXLinuxController class.
//
//////////////////////////////////////////////////////////////////////
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/process.h>
#include <wx/textfile.h>
#include "openMSXLinuxController.h"
#include "PipeReadThread.h"
#include "wxCatapultFrm.h"
#include "wxCatapultApp.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

	openMSXLinuxController::openMSXLinuxController(wxWindow * target)
:openMSXController(target)
{
	m_openMSXstdin = m_openMSXstdout = m_openMSXstderr = -1;
}

openMSXLinuxController::~openMSXLinuxController()
{
	if (m_openMsxRunning) 
		WriteCommand("quit");
	ClosePipes();
}

bool openMSXLinuxController::Launch(wxString cmdline)
{
	PreLaunch();
	cmdline += _(" -control stdio");
	if (!execute(cmdline.c_str(), m_openMSXstdin, m_openMSXstdout, m_openMSXstderr)){
		return false;
	}
	PipeReadThread * thread = new PipeReadThread(m_appWindow, MSGID_STDOUT);
	if (thread->Create() == wxTHREAD_NO_ERROR)
	{
		thread->SetFileDescriptor(m_openMSXstdout);
		thread->Run();
	}
	PipeReadThread * thread2 = new PipeReadThread(m_appWindow, MSGID_STDERR);	
	if (thread2->Create() == wxTHREAD_NO_ERROR)
	{
		thread2->SetFileDescriptor(m_openMSXstderr);
		thread2->Run();
	}
	
	m_openMsxRunning = true;
	PostLaunch();
	m_appWindow->m_launch_AbortButton->Enable(true);
	return true;
}

bool openMSXLinuxController::execute(const string& command, int& fdIn, int& fdOut, int& fdErr)
{
	// create pipes
	const int PIPE_READ = 0;
	const int PIPE_WRITE = 1;
	int pipeStdin[2], pipeStdout[2], pipeStderr[2];
	if ((pipe(pipeStdin)  == -1) ||
	    (pipe(pipeStdout) == -1) ||
	    (pipe(pipeStderr) == -1)) {
		return false;
	}

	// create new thread
	int pid = fork();
	if (pid == -1) {
		return false;
	}
	if (pid == 0) {
		// child thread
		
		// redirect IO
		close(pipeStdin[PIPE_WRITE]);
		close(pipeStdout[PIPE_READ]);
		close(pipeStderr[PIPE_READ]);
		dup2(pipeStdin[PIPE_READ], STDIN_FILENO);
		dup2(pipeStdout[PIPE_WRITE], STDOUT_FILENO);
		dup2(pipeStdout[PIPE_WRITE], STDERR_FILENO);

		// prepare cmdline
		// HACK: use sh to handle quoting
		unsigned len = command.length();
		char* cmd = static_cast<char*>(
		                alloca((len + 1) * sizeof(char)));
		memcpy(cmd, command.c_str(), len + 1);
		char* argv[4];
		argv[0] = "sh";
		argv[1] = "-c";
		argv[2] = cmd;
		argv[3] = 0;
		
		// really execute command
		execvp(argv[0], argv);

	} else {
		// parent thread
		close(pipeStdin[PIPE_READ]);
		close(pipeStdout[PIPE_WRITE]);
		close(pipeStderr[PIPE_WRITE]);

		fdIn  = pipeStdin[PIPE_WRITE];
		fdOut = pipeStdout[PIPE_READ];
		fdErr = pipeStderr[PIPE_READ];
	}
	return true;
}
 

wxString openMSXLinuxController::GetOpenMSXVersionInfo(wxString openmsxCmd)
{
	wxString version = "";
	system (wxString (openmsxCmd + " -v > /tmp/catapult.tmp").c_str());
	wxTextFile tempfile (_("/tmp/catapult.tmp"));
	if (tempfile.Open()){
		version = tempfile.GetFirstLine();
		tempfile.Close();
	}
	return wxString (version);
}

bool openMSXLinuxController::WriteMessage(wxString msg)
{
	if (!m_openMsxRunning) 
		return false;
	write(m_openMSXstdin, msg.c_str(), msg.Len());
	return true;
}

void openMSXLinuxController::HandleNativeEndProcess ()
{
	ClosePipes();
}

void openMSXLinuxController::ClosePipes ()
{
	if (m_openMSXstdin != -1){
		close (m_openMSXstdin);
	}
	if (m_openMSXstdout != -1){
		close (m_openMSXstdout);
	}
	if (m_openMSXstderr != -1){
		close (m_openMSXstderr);
	}
}
