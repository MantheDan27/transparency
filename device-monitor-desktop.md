# Device Monitor Desktop

A comprehensive Electron-based desktop application for complete system security monitoring and threat detection.

## Features

### System Scanning
- **Full System Analysis**: Scans entire computer for threats
- **Process Monitoring**: Detects suspicious processes and applications
- **Network Analysis**: Monitors active connections and suspicious traffic
- **Device Detection**: Identifies USB, Bluetooth, and network devices

### Threat Detection
- Remote access software detection (TeamViewer, AnyDesk, VNC, RDP)
- Malware and ransomware signatures
- Suspicious permission usage
- Network-based
 intrusions

### Security Actions
- **Quick Fix Buttons**: One-click remediation for detected threatsecho 'npm install status:' && ls -la node_modules | head -10

- **Automated Blocking**: Firewall integration for blocking connections
- **Service Disabling**: Stop malicious services
- **Permission Revocation**: Remove suspicious application permissions

## Requirements

- Node.js (v14 or higher)
- npm (included with Node.js)
- Windows, macOS, or Linux
- Administrator/sudo privileges for system scanning

## Installation

### Step 1: Install Node.js
Download from https://nodejs.org/ and install the LTS version.

### Step 2: Navigate to Project Directory
```bash
cd device-monitor-desktop
```

### Step 3: Install Dependencies
```bash
npm install
```

## Running the Application

### Development Mode
```bash
npm start
```

This will launch the application in development mode with DevTools open.

### Building Installers

**Windows:**
```bash
npm run build:win
```

**macOS:**
```bash
npm run build:mac
```

**Linux:**
```bash
npm run build:linux
```

## Usage

1. **Launch the Application**: Run `npm start` or open the installed executable
2. **Click "Full System Scan"**: Initiates comprehensive system analysis
3. **Review Results**: 
   - System Information: CPU, RAM, OS details
   - Detected Threats: List of security risks
   - Connected Devices: USB and network devices
   - Network Connections: Active connections
4. **Take Action**:
   - Click "Fix Now" on individual threats
   - Use "Fix All Issues" to resolve all problems
   - Export scan report for documentation

## Application Structure

```
device-monitor-desktop/
├── main.js              # Electron main process
├── package.json         # Dependencies and scripts
├── src/
│   ├── index.html      # Main HTML interface
│   ├── renderer.js     # Frontend logic
│   └── styles.css      # Styling
├── README.md           # Documentation
└── INSTALL.md          # Installation guide
```

## Key Files

- **main.js**: Electron backend with system-level access and IPC handlers
- **renderer.js**: Frontend logic for UI interactions and system scanning
- **index.html**: User interface with scan controls
- **styles.css**: Black and blue theme styling

## Security Warnings

- Run with administrator/sudo privileges for full functionality
- Only use on trusted systems
- Regularly update Node.js and dependencies
- Review detected threats carefully before fixing

## License

MIT License - See LICENSE file for details

## Support

For issues or questions, please visit: https://github.com/MantheD27/transparency
