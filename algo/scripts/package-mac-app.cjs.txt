const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const appName = 'Robot Path Finding Simulator';
const executableName = 'RobotPathFindingSimulator';
const bundleIdentifier = 'com.oyojin.robotpathfindingsimulator';

const rootDir = path.resolve(__dirname, '..');
const sourceBundle = path.join(rootDir, 'node_modules', 'electron', 'dist', 'Electron.app');
const outputDir = path.join(rootDir, 'release');
const targetBundle = path.join(outputDir, `${appName}.app`);
const distDir = path.join(rootDir, 'dist');

if (!fs.existsSync(distDir)) {
  throw new Error('Missing dist output. Run `npm run build` first.');
}

if (!fs.existsSync(sourceBundle)) {
  throw new Error('Electron binary not found. Run `npm install` first.');
}

fs.rmSync(outputDir, { recursive: true, force: true });
fs.mkdirSync(outputDir, { recursive: true });
execFileSync('ditto', [sourceBundle, targetBundle]);

const macOsDir = path.join(targetBundle, 'Contents', 'MacOS');
const resourcesDir = path.join(targetBundle, 'Contents', 'Resources');
const appDir = path.join(resourcesDir, 'app');
const plistPath = path.join(targetBundle, 'Contents', 'Info.plist');
const oldExecPath = path.join(macOsDir, 'Electron');
const newExecPath = path.join(macOsDir, executableName);

if (fs.existsSync(oldExecPath)) {
  fs.renameSync(oldExecPath, newExecPath);
}

fs.mkdirSync(appDir, { recursive: true });
fs.cpSync(distDir, path.join(appDir, 'dist'), { recursive: true });
fs.cpSync(path.join(rootDir, 'electron'), path.join(appDir, 'electron'), { recursive: true });

const packageJsonPath = path.join(appDir, 'package.json');
const rootPackageJson = JSON.parse(
  fs.readFileSync(path.join(rootDir, 'package.json'), 'utf-8')
);
const packagedVersion = rootPackageJson.version === '0.0.0' ? '1.0.0' : rootPackageJson.version;

fs.writeFileSync(
  packageJsonPath,
  JSON.stringify(
    {
      name: 'robot-path-finding-simulator',
      version: packagedVersion,
      main: 'electron/main.cjs',
    },
    null,
    2
  ) + '\n'
);

function replacePlist(key, value) {
  execFileSync('plutil', ['-replace', key, '-string', value, plistPath]);
}

replacePlist('CFBundleDisplayName', appName);
replacePlist('CFBundleName', appName);
replacePlist('CFBundleExecutable', executableName);
replacePlist('CFBundleIdentifier', bundleIdentifier);
replacePlist('CFBundleShortVersionString', packagedVersion);
replacePlist('CFBundleVersion', packagedVersion);

// Re-sign after modifying bundle contents and Info.plist.
execFileSync(
  'codesign',
  ['--force', '--deep', '--sign', '-', '--timestamp=none', targetBundle],
  { stdio: 'inherit' }
);

console.log(`Created macOS app: ${targetBundle}`);
