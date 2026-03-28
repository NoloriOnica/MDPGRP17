const path = require('path');
const { app, BrowserWindow, shell } = require('electron');

const isDev = !app.isPackaged;
const distIndexFile = path.join(__dirname, '..', 'dist', 'index.html');

async function loadRenderer(window) {
  if (isDev) {
    try {
      await window.loadURL('http://localhost:3000');
      return;
    } catch (error) {
      if (!process.env.CI) {
        console.warn('Vite dev server not available, falling back to built files.');
      }
    }
  }

  await window.loadFile(distIndexFile);
}

function createMainWindow() {
  const window = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1100,
    minHeight: 700,
    autoHideMenuBar: true,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  loadRenderer(window);

  window.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });
}

app.whenReady().then(() => {
  createMainWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createMainWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
