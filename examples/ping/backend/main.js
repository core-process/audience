const audience = require('../../../integrations/backend/nodejs').audience;
const quotes = require('./quotes');
const path = require('path');

async function main(debug) {
  // retrieve audience interface
  const app = await audience({
    icons: [16, 32, 64, 128, 512].map(icon => path.join(__dirname, '../icons', icon + '.png')),
    runtime: path.join(__dirname, '../../../dist/Debug/bin'),
    debug,
  });

  // handle window messages
  app.onWindowMessage(async ({ handle, message: command }) => {
    const [func, ...args] = command.split(/\s+/);
    switch (func) {
      // command: quote
      case 'quote': {
        await app.windowPostMessage(handle, quotes[Math.floor(Math.random() * quotes.length)]);
      } break;

      // commmand: quit
      case 'quit': {
        await app.quit();
      } break;

      // command: screens
      case 'screens': {
        const list = await app.screenList();
        const result = list.screens.map((screen, i) =>
          `Screen ${i}\n` +
          (i == list.primary ? `- Primary Screen\n` : ``) +
          (i == list.focused ? `- Focused Screen\n` : ``) +
          `- Frame: origin=${screen.frame.x},${screen.frame.y} size=${screen.frame.width},${screen.frame.height}\n` +
          `- Workspace: origin=${screen.workspace.x},${screen.workspace.y} size=${screen.workspace.width},${screen.workspace.height}\n`
        );
        await app.windowPostMessage(handle, `\n` + result.join(`\n`));
      } break;

      // command: windows
      case 'windows': {
        const list = await app.windowList();
        const result = list.windows.map((window, i) =>
          `Window ${i} with handle ${window.handle}\n` +
          (i == list.focused ? `- Focused Window\n` : ``) +
          `- Frame: origin=${window.frame.x},${window.frame.y} size=${window.frame.width},${window.frame.height}\n` +
          `- Workspace: size=${window.workspace.width},${window.workspace.height}\n`
        );
        await app.windowPostMessage(handle, `\n` + result.join(`\n`));
      } break;

      // command: pos
      case 'pos': {
        const screens = await app.screenList();
        const ws = screens.screens[screens.focused].workspace;
        let target = { ...ws };
        switch (args.length > 0 ? args[0] : null) {
          case 'left': {
            target.width *= 0.5;
          } break;
          case 'top': {
            target.height *= 0.5;
          } break;
          case 'right': {
            target.width *= 0.5;
            target.x += ws.width - target.width;
          } break;
          case 'bottom': {
            target.height *= 0.5;
            target.y += ws.height - target.height;
          } break;
          case 'center': {
            target.width *= 0.5;
            target.height *= 0.5;
            target.x += (ws.width - target.width) * 0.5;
            target.y += (ws.height - target.height) * 0.5;
          } break;
          default: {
            target = null;
            await app.windowPostMessage(handle, `Unknown position: ${args.join(' ')}`);
          }
        }
        if (target) {
          await app.windowUpdatePosition(handle, target);
        }
      } break;

      // unknown command
      default: {
        await app.windowPostMessage(handle, `Unknown command: ${command}`);
      }
    }
  });

  // create application window
  app.windowCreate({ dir: path.join(__dirname, '../webapp') });

  // wait for future exit of app
  // not required, but nice to manage lifecycle of main()
  await app.futureExit();
};

main(
  process.argv.indexOf('--debug') !== -1
)
  .catch((error) => { console.error('error', error); process.exit(1); })
  .then(() => { console.log('completed'); });
