const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');

let mainWindow;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 265,
        height: 475,
        frame: false,
        resizable: false,
        title: 'QQ秀2000',
        icon: path.join(__dirname, 'assets/icons/qqshow_icon.PNG'),
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html');
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
