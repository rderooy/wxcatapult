// MiscControlPage.h: interface for the MiscControlPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MISCCONTROLPAGE_H__268AE890_7734_45CE_8C8C_55AD2ADB2157__INCLUDED_)
#define AFX_MISCCONTROLPAGE_H__268AE890_7734_45CE_8C8C_55AD2ADB2157__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <wx/tglbtn.h>
#include <wx/slider.h>

class wxCatapultFrame;
class openMSXController;
class MiscControlPage : public wxPanel  
{
	public:
		void SetLaunchDefaults();
		void DisableControls();
		void EnableControls ();
		MiscControlPage(wxWindow * parent = (wxWindow *)NULL, openMSXController * controller=NULL);
		virtual ~MiscControlPage();
		void OnSetThrottle (wxCommandEvent &event);
		void OnSetCmdTiming (wxCommandEvent &event);	
		void OnSetNormalSpeed (wxCommandEvent & event);
		void OnSetMaxSpeed (wxCommandEvent & event);
		void OnSpeedChange (wxScrollEvent & event);
		void OnSetAutoFrameSkip (wxCommandEvent & event);
		void OnSetZeroFrameSkip (wxCommandEvent & event);
		void OnFrameSkipChange (wxScrollEvent & event);
		void OnFirmware (wxCommandEvent & event);
		void OnPower (wxCommandEvent &event);
		void OnPause (wxCommandEvent &event);
		void OnReset (wxCommandEvent &event);
		void OnInputFrameskip (wxCommandEvent & event);
		void OnInputSpeed (wxCommandEvent & event);
		void OnMute (wxCommandEvent & event);
		wxToggleButton * m_powerButton;

	private:
		openMSXController * m_controller;
		wxButton * m_resetButton;
		wxButton * m_speedNormalButton;
		wxButton * m_speedMaxButton;
		wxButton * m_frameSkipZeroButton;

		wxToggleButton * m_pauseButton;
		wxToggleButton * m_firmwareButton;
		wxToggleButton * m_frameSkipAutoButton;
		wxToggleButton * m_throttleButton;
		wxToggleButton * m_cmdTimingButton;
		wxToggleButton * m_muteButton;
		wxSlider * m_speedSlider;
		wxSlider * m_frameSkipSlider;
		wxTextCtrl * m_speedIndicator;
		wxTextCtrl * m_frameSkipIndicator;

		DECLARE_CLASS(MiscControlPage)
			// any class wishing to process wxWindows events must use this macro
			DECLARE_EVENT_TABLE()
};

#endif // !defined(AFX_MISCCONTROLPAGE_H__268AE890_7734_45CE_8C8C_55AD2ADB2157__INCLUDED_)
