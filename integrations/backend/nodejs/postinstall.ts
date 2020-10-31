import fs from 'fs';
import path from 'path';
import download from 'download';
import os from 'os';

const version = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json'), 'utf8')).version;

if (version == '0.0.0') {
  console.info('Developer mode detected, skip installing runtime.');
  process.exit(0);
}

const packageUrls: { [key: string]: string; } = {
  darwin: `https://github.com/coreprocess/audience/releases/download/v${version}/audience-v${version}-macos-x64-runtime-cli.tar.gz`,
  linux: `https://github.com/coreprocess/audience/releases/download/v${version}/audience-v${version}-linux-scpprt-x64-runtime-cli.tar.gz`,
  win32: `https://github.com/coreprocess/audience/releases/download/v${version}/audience-v${version}-windows-scrt-x64-runtime-cli.zip`,
};

const platform = os.platform();
const packageUrl = packageUrls[platform];

if (packageUrl === undefined) {
  console.error(`Platform ${platform} currently supported. Please contact maintainer.`);
  process.exit(1);
}

console.info(`Downloading ${packageUrl} now...`);

download(packageUrl, path.join(__dirname, 'runtime'), { extract: true, strip: 1 })
  .then(() => {
    console.info(`Provisioning of runtime completed successfully.`);
    process.exit(0);
  })
  .catch((error) => {
    console.info(`Could not provision runtime. An error occured: ${(error && error.message) || error}`);
  });
