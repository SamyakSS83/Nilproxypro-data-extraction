#include <ncurses.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <vector>
#include <string>

#define BUFFER_SIZE 1024

// Structure to hold a serial port
struct SerialPort {
    std::string displayName;
    std::string fullPath;
};

// Find available serial ports on Linux
std::vector<SerialPort> findSerialPorts() {
    std::vector<SerialPort> ports;
    DIR *dir;
    struct dirent *ent;
    
    if ((dir = opendir("/dev/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0) {
                SerialPort port;
                port.displayName = name;
                port.fullPath = "/dev/" + name;
                ports.push_back(port);
            }
        }
        closedir(dir);
    }
    
    return ports;
}

// Open serial port with given settings
int openSerialPort(const char *device) {
    int fd = open(device, O_RDONLY | O_NOCTTY);
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

int main() {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Colors
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);    // Header
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Success
    init_pair(3, COLOR_RED, COLOR_BLACK);     // Error
    
    // Find serial ports
    std::vector<SerialPort> ports = findSerialPorts();
    
    // Main window
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    
    // Create windows
    WINDOW *headerWin = newwin(3, maxX, 0, 0);
    WINDOW *menuWin = newwin(ports.size() + 4, maxX/3, 3, 0);
    WINDOW *outputWin = newwin(maxY - 3, maxX - maxX/3, 3, maxX/3);
    
    // Header
    wbkgd(headerWin, COLOR_PAIR(1));
    mvwprintw(headerWin, 1, maxX/2 - 10, "ESP32 SERIAL MONITOR");
    wrefresh(headerWin);
    
    // Menu
    box(menuWin, 0, 0);
    mvwprintw(menuWin, 1, 2, "Available Ports:");
    
    if (ports.empty()) {
        mvwprintw(menuWin, 3, 2, "No ports found");
        wrefresh(menuWin);
        
        mvwprintw(stdscr, maxY-1, 0, "Press any key to exit");
        refresh();
        getch();
        endwin();
        return 0;
    }
    
    for (size_t i = 0; i < ports.size(); i++) {
        mvwprintw(menuWin, i + 3, 2, "%zu. %s", i + 1, ports[i].displayName.c_str());
    }
    
    mvwprintw(menuWin, ports.size() + 3, 2, "Press q to quit");
    wrefresh(menuWin);
    
    // Output window
    box(outputWin, 0, 0);
    mvwprintw(outputWin, 1, 2, "Serial Output:");
    wrefresh(outputWin);
    
    // Select port
    int chosen = 0;
    mvwprintw(menuWin, ports.size() + 2, 2, "Select port (1-%zu): ", ports.size());
    wrefresh(menuWin);
    
    // Get user input for port selection
    char choice[10];
    echo();
    wgetnstr(menuWin, choice, 2);
    noecho();
    
    if (choice[0] == 'q') {
        endwin();
        return 0;
    }
    
    chosen = atoi(choice) - 1;
    if (chosen < 0 || chosen >= (int)ports.size()) {
        mvwprintw(outputWin, 2, 2, "Invalid selection");
        wrefresh(outputWin);
        mvwprintw(stdscr, maxY-1, 0, "Press any key to exit");
        refresh();
        getch();
        endwin();
        return 0;
    }
    
    // Open selected port
    int serialFd = openSerialPort(ports[chosen].fullPath.c_str());
    if (serialFd < 0) {
        wattron(outputWin, COLOR_PAIR(3));
        mvwprintw(outputWin, 2, 2, "Failed to open %s", ports[chosen].fullPath.c_str());
        wattroff(outputWin, COLOR_PAIR(3));
        wrefresh(outputWin);
        mvwprintw(stdscr, maxY-1, 0, "Press any key to exit");
        refresh();
        getch();
        endwin();
        return 0;
    }
    
    // Success message
    wattron(outputWin, COLOR_PAIR(2));
    mvwprintw(outputWin, 2, 2, "Connected to %s", ports[chosen].fullPath.c_str());
    wattroff(outputWin, COLOR_PAIR(2));
    mvwprintw(outputWin, 3, 2, "Press q to exit");
    wrefresh(outputWin);
    
    // Non-blocking input
    nodelay(stdscr, TRUE);
    
    // Read loop
    bool running = true;
    char buffer[BUFFER_SIZE];
    std::string dataBuffer;
    int line = 4;
    
    while (running) {
        // Check for user input
        int ch = getch();
        if (ch == 'q') {
            running = false;
            continue;
        }
        
        // Read from serial port
        ssize_t n = read(serialFd, buffer, BUFFER_SIZE - 1);
        if (n > 0) {
            buffer[n] = '\0';
            dataBuffer += buffer;
            
            // Process messages with START/END markers
            size_t startPos = dataBuffer.find("START");
            size_t endPos = dataBuffer.find("END");
            
            while (startPos != std::string::npos && endPos != std::string::npos && startPos < endPos) {
                std::string message = dataBuffer.substr(startPos + 5, endPos - startPos - 5);
                
                // Display message
                if (line >= maxY - 5) {
                    // Clear output area when reaching bottom
                    for (int i = 4; i < maxY - 4; i++) {
                        mvwprintw(outputWin, i, 2, "%*s", maxX - maxX/3 - 4, "");
                    }
                    line = 4;
                }
                
                mvwprintw(outputWin, line++, 2, "Received:");
                
                // Split message by newlines
                size_t pos = 0;
                std::string token;
                while ((pos = message.find("\n")) != std::string::npos) {
                    token = message.substr(0, pos);
                    mvwprintw(outputWin, line++, 4, "%s", token.c_str());
                    message.erase(0, pos + 1);
                }
                if (!message.empty()) {
                    mvwprintw(outputWin, line++, 4, "%s", message.c_str());
                }
                
                line++; // Add blank line between messages
                
                // Remove processed data
                dataBuffer.erase(0, endPos + 3);
                
                // Find next message
                startPos = dataBuffer.find("START");
                endPos = dataBuffer.find("END");
            }
            
            wrefresh(outputWin);
        }
        
        // Small delay to prevent CPU hogging
        usleep(50000);
    }
    
    // Clean up
    close(serialFd);
    delwin(headerWin);
    delwin(menuWin);
    delwin(outputWin);
    endwin();
    
    return 0;
}