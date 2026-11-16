#!/usr/bin/env node
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import { homedir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sourceDir = path.join(root, 'cpp', 'tests');
const buildDir = path.join(root, 'build', 'native-tests');
const isWin = process.platform === 'win32';
const exe = (name) => (isWin ? `${name}.exe` : name);

function onPath(name) {
  const probe = spawnSync(isWin ? 'where' : 'which', [name], {
    encoding: 'utf8',
  });
  if (probe.status !== 0) return null;
  return probe.stdout.split(/\r?\n/).find(Boolean) ?? null;
}

function compareVersions(a, b) {
  const pa = a.split('.').map(Number);
  const pb = b.split('.').map(Number);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const d = (pa[i] ?? 0) - (pb[i] ?? 0);
    if (d !== 0) return d;
  }
  return 0;
}

function androidSdkCmakeBins() {
  const sdkRoots = [
    process.env.ANDROID_HOME,
    process.env.ANDROID_SDK_ROOT,
    process.env.LOCALAPPDATA &&
      path.join(process.env.LOCALAPPDATA, 'Android', 'sdk'),
    path.join(homedir(), 'Library', 'Android', 'sdk'),
    path.join(homedir(), 'Android', 'Sdk'),
  ].filter(Boolean);
  for (const sdk of sdkRoots) {
    const cmakeRoot = path.join(sdk, 'cmake');
    if (!existsSync(cmakeRoot)) continue;
    const versions = readdirSync(cmakeRoot)
      .filter((v) => /^\d+(\.\d+)*$/.test(v))
      .sort(compareVersions)
      .reverse();
    for (const v of versions) {
      const bin = path.join(cmakeRoot, v, 'bin');
      if (existsSync(path.join(bin, exe('cmake')))) return bin;
    }
  }
  return null;
}

function findTool(name, fallbackBin) {
  const fromPath = onPath(name);
  if (fromPath) return fromPath;
  if (fallbackBin && existsSync(path.join(fallbackBin, exe(name)))) {
    return path.join(fallbackBin, exe(name));
  }
  return null;
}

// why: a plain Windows shell has no `cl` on PATH; vcvars64 provides INCLUDE/LIB/PATH.
function msvcEnv() {
  const vswhere = path.join(
    process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)',
    'Microsoft Visual Studio',
    'Installer',
    'vswhere.exe'
  );
  if (!existsSync(vswhere)) {
    throw new Error('MSVC not found: install Visual Studio Build Tools (C++).');
  }
  const installPath = execFileSync(
    vswhere,
    [
      '-latest',
      '-products',
      '*',
      '-requires',
      'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
      '-property',
      'installationPath',
    ],
    { encoding: 'utf8' }
  ).trim();
  const vcvars = path.join(
    installPath,
    'VC',
    'Auxiliary',
    'Build',
    'vcvars64.bat'
  );
  if (!installPath || !existsSync(vcvars)) {
    throw new Error('vcvars64.bat not found in the latest Visual Studio.');
  }
  const out = execFileSync(
    'cmd.exe',
    ['/d', '/s', '/c', `"call "${vcvars}" >nul && set"`],
    {
      encoding: 'utf8',
      windowsVerbatimArguments: true,
      env: {
        ...process.env,
        PATH: `${path.dirname(vswhere)};${process.env.PATH ?? ''}`,
      },
    }
  );
  const env = {};
  for (const line of out.split(/\r?\n/)) {
    const eq = line.indexOf('=');
    if (eq > 0) env[line.slice(0, eq)] = line.slice(eq + 1);
  }
  return env;
}

function run(cmd, args, env) {
  console.log(`> ${path.basename(cmd)} ${args.join(' ')}`);
  const r = spawnSync(cmd, args, { stdio: 'inherit', env });
  if (r.error) throw r.error;
  if (r.status !== 0) process.exit(r.status ?? 1);
}

const sdkBin = androidSdkCmakeBins();
const cmake = findTool('cmake', sdkBin);
if (!cmake) {
  console.error(
    'cmake not found on PATH or in $ANDROID_HOME/cmake/<version>/bin.'
  );
  process.exit(1);
}
const cmakeBin = path.dirname(cmake);
const ctest = path.join(cmakeBin, exe('ctest'));
const ninja = findTool('ninja', sdkBin) ?? findTool('ninja', cmakeBin);

let env = process.env;
if (isWin && !onPath('cl')) env = { ...process.env, ...msvcEnv() };

const configureArgs = ['-S', sourceDir, '-B', buildDir];
const multiConfig = !ninja;
if (ninja) {
  configureArgs.push(
    '-G',
    'Ninja',
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    '-DCMAKE_BUILD_TYPE=Release'
  );
  if (isWin) {
    configureArgs.push('-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl');
  }
}

run(cmake, configureArgs, env);
run(
  cmake,
  [
    '--build',
    buildDir,
    '--parallel',
    ...(multiConfig ? ['--config', 'Release'] : []),
  ],
  env
);
run(
  ctest,
  [
    '--test-dir',
    buildDir,
    '--output-on-failure',
    ...(multiConfig ? ['-C', 'Release'] : []),
    ...process.argv.slice(2),
  ],
  env
);
