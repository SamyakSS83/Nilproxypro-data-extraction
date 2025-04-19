#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/statbox.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>

#include <vector>
#include <string>
#include <dirent.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// Structure to hold a serial port
struct SerialPort {
    std::string displayName;
    std::string fullPath;
};

// Application class
class SerialMonitorApp : public wxApp {
public:
    virtual bool OnInit();
};

// Main frame class
class SerialMonitorFrame : public wxFrame {
public:
    SerialMonitorFrame(const wxString& title);
    virtual ~SerialMonitorFrame();

private:
    // GUI components
    wxListBox* m_portList;
    wxTextCtrl* m_outputText;
    wxButton* m_connectButton;
    wxButton* m_disconnectButton;
    wxStaticText* m_statusText;
    wxMenu* m_contextMenu;
    
    // Serial port handling
    std::vector<SerialPort> m_ports;
    int m_serialFd;
    bool m_connected;
    wxTimer m_timer;
    
    // Methods
    void FindSerialPorts();
    int OpenSerialPort(const std::string& device);
    void CloseSerialPort();
    
    // Event handlers
    void OnConnect(wxCommandEvent& event);
    void OnDisconnect(wxCommandEvent& event);
    void OnRefreshPorts(wxCommandEvent& event);
    void OnClearOutput(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnPortRightClick(wxMouseEvent& event);
    void OnContextMenuSelected(wxCommandEvent& event);
    
    DECLARE_EVENT_TABLE()
};

// Event table for SerialMonitorFrame
BEGIN_EVENT_TABLE(SerialMonitorFrame, wxFrame)
    EVT_BUTTON(wxID_OK, SerialMonitorFrame::OnConnect)
    EVT_BUTTON(wxID_CANCEL, SerialMonitorFrame::OnDisconnect)
    EVT_TIMER(wxID_ANY, SerialMonitorFrame::OnTimer)
END_EVENT_TABLE()

wxIMPLEMENT_APP(SerialMonitorApp);

bool SerialMonitorApp::OnInit() {
    SerialMonitorFrame* frame = new SerialMonitorFrame("ESP32 Serial Monitor");
    frame->Show(true);
    return true;
}

SerialMonitorFrame::SerialMonitorFrame(const wxString& title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(800, 600)),
      m_serialFd(-1), m_connected(false), m_timer(this) {
    
    // Create a status bar
    CreateStatusBar();
    SetStatusText("Welcome to ESP32 Serial Monitor");
    
    // Create main panel with sizers for layout
    wxPanel* panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Left panel for port selection
    wxStaticBoxSizer* leftSizer = new wxStaticBoxSizer(wxVERTICAL, panel, "Available Ports");
    m_portList = new wxListBox(panel, wxID_ANY);
    m_portList->Connect(wxEVT_RIGHT_DOWN, 
                        wxMouseEventHandler(SerialMonitorFrame::OnPortRightClick), 
                        NULL, this);
    
    m_connectButton = new wxButton(panel, wxID_OK, "Connect");
    m_disconnectButton = new wxButton(panel, wxID_CANCEL, "Disconnect");
    m_disconnectButton->Disable();
    
    wxButton* refreshButton = new wxButton(panel, wxID_REFRESH, "Refresh Ports");
    refreshButton->Bind(wxEVT_BUTTON, &SerialMonitorFrame::OnRefreshPorts, this);
    
    leftSizer->Add(m_portList, 1, wxEXPAND | wxALL, 5);
    leftSizer->Add(m_connectButton, 0, wxEXPAND | wxALL, 5);
    leftSizer->Add(m_disconnectButton, 0, wxEXPAND | wxALL, 5);
    leftSizer->Add(refreshButton, 0, wxEXPAND | wxALL, 5);
    
    // Right panel for serial output
    wxStaticBoxSizer* rightSizer = new wxStaticBoxSizer(wxVERTICAL, panel, "Serial Output");
    m_outputText = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, 
                                  wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    
    wxButton* clearButton = new wxButton(panel, wxID_CLEAR, "Clear Output");
    clearButton->Bind(wxEVT_BUTTON, &SerialMonitorFrame::OnClearOutput, this);
    
    rightSizer->Add(m_outputText, 1, wxEXPAND | wxALL, 5);
    rightSizer->Add(clearButton, 0, wxALIGN_RIGHT | wxALL, 5);
    
    // Status text
    m_statusText = new wxStaticText(panel, wxID_ANY, "Not connected");
    rightSizer->Add(m_statusText, 0, wxALL, 5);
    
    // Add sizers to main sizer
    mainSizer->Add(leftSizer, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(rightSizer, 2, wxEXPAND | wxALL, 5);
    
    panel->SetSizer(mainSizer);
    
    // Create context menu for right-click
    m_contextMenu = new wxMenu();
    m_contextMenu->Append(wxID_REFRESH, "Refresh Ports");
    m_contextMenu->Append(wxID_PROPERTIES, "Port Details");
    
    // Connect the context menu to event handler
    Connect(wxID_REFRESH, wxEVT_COMMAND_MENU_SELECTED,
            wxCommandEventHandler(SerialMonitorFrame::OnContextMenuSelected));
    Connect(wxID_PROPERTIES, wxEVT_COMMAND_MENU_SELECTED,
            wxCommandEventHandler(SerialMonitorFrame::OnContextMenuSelected));
    
    // Initial port scan
    FindSerialPorts();
    
    // Center the window
    Centre();
}

SerialMonitorFrame::~SerialMonitorFrame() {
    CloseSerialPort();
    delete m_contextMenu;
}

void SerialMonitorFrame::FindSerialPorts() {
    m_ports.clear();
    m_portList->Clear();
    
    DIR *dir;
    struct dirent *ent;
    
    if ((dir = opendir("/dev/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0) {
                SerialPort port;
                port.displayName = name;
                port.fullPath = "/dev/" + name;
                m_ports.push_back(port);
                m_portList->Append(port.displayName);
            }
        }
        closedir(dir);
    }
    
    if (m_ports.empty()) {
        m_portList->Append("No ports found");
        m_connectButton->Disable();
    } else {
        m_connectButton->Enable();
        m_portList->SetSelection(0);
    }
}

int SerialMonitorFrame::OpenSerialPort(const std::string& device) {
    int fd = open(device.c_str(), O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    // Set baud rate to 115200
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    // 8N1 mode, no flow control
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    
    // Turn on READ & ignore ctrl lines
    tty.c_cflag |= CREAD | CLOCAL;
    
    // Raw input
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    
    // Raw output
    tty.c_oflag &= ~OPOST;
    
    // Non-blocking read
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

void SerialMonitorFrame::CloseSerialPort() {
    if (m_serialFd != -1) {
        close(m_serialFd);
        m_serialFd = -1;
    }
    
    if (m_timer.IsRunning()) {
        m_timer.Stop();
    }
    
    m_connected = false;
}

void SerialMonitorFrame::OnConnect(wxCommandEvent& event) {
    int selection = m_portList->GetSelection();
    if (selection == wxNOT_FOUND || m_ports.empty()) {
        wxMessageBox("Please select a valid port", "Error", 
                     wxOK | wxICON_ERROR, this);
        return;
    }
    
    m_serialFd = OpenSerialPort(m_ports[selection].fullPath);
    if (m_serialFd < 0) {
        wxMessageDialog* errorDialog = new wxMessageDialog(
            this,
            "Failed to open " + m_ports[selection].fullPath,
            "Connection Error",
            wxOK | wxICON_ERROR
        );
        errorDialog->ShowModal();
        errorDialog->Destroy();
        return;
    }
    
    m_connected = true;
    m_connectButton->Disable();
    m_disconnectButton->Enable();
    m_statusText->SetLabel("Connected to " + m_ports[selection].displayName);
    m_outputText->AppendText("Connected to " + m_ports[selection].displayName + "\n");
    
    // Start timer to poll for serial data
    m_timer.Start(100);  // Poll every 100ms
}

void SerialMonitorFrame::OnDisconnect(wxCommandEvent& event) {
    CloseSerialPort();
    m_connectButton->Enable();
    m_disconnectButton->Disable();
    m_statusText->SetLabel("Disconnected");
    m_outputText->AppendText("Disconnected\n");
}

void SerialMonitorFrame::OnRefreshPorts(wxCommandEvent& event) {
    FindSerialPorts();
}

void SerialMonitorFrame::OnClearOutput(wxCommandEvent& event) {
    m_outputText->Clear();
}

void SerialMonitorFrame::OnTimer(wxTimerEvent& event) {
    if (!m_connected || m_serialFd < 0) {
        return;
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t n = read(m_serialFd, buffer, BUFFER_SIZE - 1);
    
    if (n > 0) {
        buffer[n] = '\0';
        wxString data(buffer, wxConvUTF8);
        m_outputText->AppendText(data);
    }
}

void SerialMonitorFrame::OnPortRightClick(wxMouseEvent& event) {
    // Show context menu on right click
    PopupMenu(m_contextMenu, event.GetPosition());
}

void SerialMonitorFrame::OnContextMenuSelected(wxCommandEvent& event) {
    int id = event.GetId();
    
    if (id == wxID_REFRESH) {
        FindSerialPorts();
    } 
    else if (id == wxID_PROPERTIES) {
        int selection = m_portList->GetSelection();
        if (selection != wxNOT_FOUND && !m_ports.empty()) {
            wxMessageDialog* infoDialog = new wxMessageDialog(
                this,
                "Port: " + m_ports[selection].fullPath + "\n" +
                "Type: Serial USB Device",
                "Port Details",
                wxOK | wxICON_INFORMATION
            );
            infoDialog->ShowModal();
            infoDialog->Destroy();
        }
    }
}
