#include "stdafx.h"
#include "../NovacMasterProgram.h"
#include "ManualWindDlg.h"

// the settings...
#include "../Configuration/Configuration.h"

#include "../Communication/CommunicationController.h"

using namespace Dialogs;

extern CConfigurationSetting g_settings;	// <-- The settings
extern CWinThread *g_comm;								// <-- The communication controller

IMPLEMENT_DYNAMIC(CManualWindDlg, CDialog)
CManualWindDlg::CManualWindDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CManualWindDlg::IDD, pParent)
{
    m_specNum = 0;
    m_motorPosition[0] = 0;
    m_motorPosition[1] = 0;
    m_sum1 = 5;
    m_repetitions = 750;
    m_stepsPerRound[0] = 200;
    m_stepsPerRound[1] = 3600;
    m_motorStepsComp[0] = 85;
    m_motorStepsComp[1] = 85;
    m_maxExpTime = 300;
    m_compass = 0.0;
    m_percent = 0.8;
}

CManualWindDlg::~CManualWindDlg()
{
}

void CManualWindDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_SPECTROMETER_LIST, m_spectrometerList);
    DDX_Control(pDX, IDC_EDIT_MOTORPOSITION, m_editMotorPosition);
    DDX_Control(pDX, IDC_EDIT_MOTORPOSITION2, m_editMotorPosition2);
    DDX_Control(pDX, IDC_EDIT_STEPSPERROUND, m_editStepsPerRound);
    DDX_Control(pDX, IDC_EDIT_STEPSPERROUND2, m_editStepsPerRound2);
    DDX_Control(pDX, IDC_EDIT_MOTORSTEPSCOMP, m_editMotorStepsComp);
    DDX_Control(pDX, IDC_EDIT_MOTORSTEPSCOMP2, m_editMotorStepsComp2);

    DDX_Text(pDX, IDC_EDIT_MOTORPOSITION, m_motorPosition[0]);
    DDX_Text(pDX, IDC_EDIT_MOTORPOSITION2, m_motorPosition[1]);
    DDX_Text(pDX, IDC_EDIT_SUM1, m_sum1);
    DDX_Text(pDX, IDC_EDIT_REPETITIONS, m_repetitions);
    DDX_Text(pDX, IDC_EDIT_MOTORSTEPSCOMP, m_motorStepsComp[0]);
    DDX_Text(pDX, IDC_EDIT_MOTORSTEPSCOMP2, m_motorStepsComp[1]);
    DDX_Text(pDX, IDC_EDIT_STEPSPERROUND, m_stepsPerRound[0]);
    DDX_Text(pDX, IDC_EDIT_STEPSPERROUND2, m_stepsPerRound[1]);
    DDX_Text(pDX, IDC_EDIT_COMPASS, m_compass);
    DDX_Text(pDX, IDC_EDIT_PERCENT, m_percent);
}


BEGIN_MESSAGE_MAP(CManualWindDlg, CDialog)
    ON_BN_CLICKED(IDOK, OnSend)

    // Changing the selected scanner
    ON_LBN_SELCHANGE(IDC_SPECTROMETER_LIST, OnChangeSpectrometer)
END_MESSAGE_MAP()


// CManualWindDlg message handlers
/** Called to initialize the controls in the dialog*/
BOOL CManualWindDlg::OnInitDialog() {

    CDialog::OnInitDialog();

    /** Setting up the list */
    for (unsigned int k = 0; k < g_settings.scannerNum; ++k) {
        for (unsigned int i = 0; i < g_settings.scanner[k].specNum; ++i) {
            if (g_settings.scanner[k].spec[i].channelNum <= 1) {
                continue;
            }

            // if this spectrometer contains several channels, or is a Heidelberg instrument then add it to the lists
            m_spectrometer[m_specNum].Format(g_settings.scanner[k].spec[i].serialNumber);
            m_spectrometerList.AddString(m_spectrometer[m_specNum]);
            ++m_specNum;
        }
    }

    // Select the first spectrometer in the list
    if (m_specNum > 0)
        m_spectrometerList.SetCurSel(0);

    // Set the focus to the motor-position edit-box
    m_editMotorPosition.SetFocus();

    // Decide which edit boxes to show...
    OnChangeSpectrometer();

    return FALSE; //<-- return false if we've set the focus to a control
}

/** When the user presses the 'Send' - button */
void CManualWindDlg::OnSend() {
    CString *fileName = new CString();
    CString *serialNumber = new CString();
    CString dateTime;
    Common common;

    // 1. Get values the user has written in the dialog
    UpdateData(TRUE);

    // 2. Get a handle to the selected spectrometer
    int curSel = m_spectrometerList.GetCurSel();
    if (curSel < 0) {
        MessageBox("Please Select a spectrometer");
        return;
    }
    serialNumber->Format(m_spectrometer[curSel]);
    CConfigurationSetting::ScanningInstrumentSetting *scanner = NULL;
    for (unsigned int k = 0; k < g_settings.scannerNum; ++k) {
        if (Equals(g_settings.scanner[k].spec[0].serialNumber, *serialNumber)) {
            scanner = &g_settings.scanner[k];
            break;
        }
    }
    if (!scanner) {
        MessageBox("Could not get scanner properties. Upload failed!!", "Error");
        return;
    }

    // 3. Get the directory where to temporarily store the cfgonce.txt
    if (strlen(g_settings.outputDirectory) > 0) {
        fileName->Format("%s\\Temp\\cfgonce.txt", (LPCSTR)g_settings.outputDirectory);
    }
    else {
        common.GetExePath();
        fileName->Format("%s\\cfgonce.txt", (LPCSTR)common.m_exePath);
    }
    FILE *f = fopen(*fileName, "w");
    if (f == NULL) {
        MessageBox("Could not open cfgonce.txt for writing. Upload failed!!", "Error");
        return;
    }

    // 4. Write the configuration-file

    // 4a. A small header 
    common.GetDateTimeText(dateTime);
    fprintf(f, "%% -------------Modified at %s------------\n\n", (LPCSTR)dateTime);

    // 4c. Write the Spectrum transfer information
    fprintf(f, "%% The following channels defines which channels in the spectra that will be transferred\n");
    fprintf(f, "STARTCHN=0\n");
    fprintf(f, "STOPCHN=684\n\n");

    // 4d. Don't use real-time collection
    fprintf(f, "%% If Realtime=1 then the spectra will be added to work.pak one at a time.\n");
    fprintf(f, "%% If RealTime=0 then the spectra will be added to work.pak one scan at a time\n");
    fprintf(f, "REALTIME=0\n\n");

    // 4e. Write the motor information
    fprintf(f, "%% StepsPerRound defines the number of steps the steppermotor divides one round into\n");
    fprintf(f, "STEPSPERROUND=%d\n", m_stepsPerRound[0]);
    fprintf(f, "MOTORSTEPCOMP=%d\n\n", m_motorStepsComp[0]);
    fprintf(f, "%% If Skipmotor=1 then the scanner will not be used. ONLY FOR TESTING PURPOSES\n");
    fprintf(f, "SKIPMOTOR=0\n");
    fprintf(f, "DELAY=%d\n\n", 400);

    // 4f. Write the geometry (compass, tilt...)
    fprintf(f, "%% The geometry: compassDirection  tiltX(=roll)  tiltY(=pitch)  temperature\n");
    fprintf(f, "COMPASS=%.1lf %.1lf %.1lf\n\n", m_compass, 0.0, 0.0);

    // 4g. Write other things
    fprintf(f, "%% Percent defines how big part of the spectrometers dynamic range we want to use\n");
    fprintf(f, "PERCENT=%.2lf\n\n", m_percent);
    fprintf(f, "%% The maximum integration time that we allow the spectrometer to use. In milli seconds\n");
    fprintf(f, "MAXINTTIME=%.0lf\n\n", m_maxExpTime);
    fprintf(f, "%% The pixel where we want to measure the intensity of the spectra \n");
    fprintf(f, "CHANNEL=%.0lf\n\n", 670.0);
    fprintf(f, "%% The debug-level, the higher number the more output will be created\n");
    fprintf(f, "DEBUG=1\n\n");

    // 4h. Write the measurement information
    fprintf(f, "%% sum1 is inside the specrometer [1 to 15]\n%%-----pos----time-sum1-sum2--chn--basename----- repetitions\n");

    // 4i. The sky-measurement
    fprintf(f, "MEAS=0 -1 15 1 257 sky 1 0\n");

    // 4j. The dark-measurement
    fprintf(f, "MEAS=100 0 15 1 257 dark 1 0\n");

    // 4k. The actual measurement
    fprintf(f, "MEAS=%d 0 %d 1 257 wind %d 0\n", m_motorPosition[0], m_sum1, m_repetitions);

    // 4l. Another dark-measurement
    fprintf(f, "MEAS=100 0 15 1 257 dark 1 0\n");

    // Close the file
    fclose(f);

    // Tell the communication controller that we want to upload a file
    if (g_comm != NULL)
    {
        g_comm->PostThreadMessage(WM_UPLOAD_CFGONCE, (WPARAM)serialNumber, (LPARAM)fileName);
    }

    CDialog::OnOK();
}

/** When the user presses changes the selected spectrometer */
void CManualWindDlg::OnChangeSpectrometer() {
    CString serialNumber;

    // 1. Get values the user has written in the dialog
    UpdateData(TRUE);

    // 2. Get a handle to the selected spectrometer
    int curSel = m_spectrometerList.GetCurSel();
    if (curSel < 0) {
        return;
    }
    serialNumber.Format(m_spectrometer[curSel]);
    CConfigurationSetting::ScanningInstrumentSetting *scanner = NULL;
    for (unsigned int k = 0; k < g_settings.scannerNum; ++k) {
        if (Equals(g_settings.scanner[k].spec[0].serialNumber, serialNumber)) {
            scanner = &g_settings.scanner[k];
            break;
        }
    }
    if (!scanner) {
        return;
    }

    m_editMotorPosition2.ShowWindow(FALSE);
    m_editStepsPerRound2.ShowWindow(FALSE);
    m_editMotorStepsComp2.ShowWindow(FALSE);
}
