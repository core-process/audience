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
  if (debug) console.debug('audience() completed');

  // handle close related events
  app.onWindowCloseIntent(async ({ handle }) => {
    await app.windowDestroy(handle);
    if (debug) console.debug('app.windowDestroy() completed');
  });

  app.onWindowClose(async ({ is_last_window }) => {
    if (is_last_window) {
      await app.quit();
      if (debug) console.debug('app.quit() completed');
    }
  });

  // handle window messages
  app.onWindowMessage(async ({ handle, message: command }) => {
    const [func, ...args] = command.split(/\s+/);
    switch (func) {
      // command: quote
      case 'quote': {
        await app.windowPostMessage(handle, quotes[Math.floor(Math.random() * quotes.length)]);
        if (debug) console.debug('app.windowPostMessage() completed');
      } break;

      // command: open
      case 'open': {
        await app.windowCreate({ dir: path.join(__dirname, '../webapp'), dev: true });
        if (debug) console.debug('app.windowCreate() completed');
      } break;

      // commmand: quit
      case 'quit': {
        await app.quit();
        if (debug) console.debug('app.quit() completed');
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
        if (debug) console.debug('app.windowPostMessage() completed');
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
        if (debug) console.debug('app.windowPostMessage() completed');
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
            if (debug) console.debug('app.windowPostMessage() completed');
          }
        }
        if (target) {
          await app.windowUpdatePosition(handle, target);
          if (debug) console.debug('app.windowUpdatePosition() completed');
        }
      } break;

      // unknown command
      default: {
        await app.windowPostMessage(handle, `Unknown command: ${command}`);
        if (debug) console.debug('app.windowPostMessage() completed');
      }
    }
  });

  // create application window
  await app.windowCreate({ dir: path.join(__dirname, '../webapp'), dev: true });
  if (debug) console.debug('app.windowCreate() completed');

  // wait for future exit of app
  // not required, but nice to manage lifecycle of main()
  await app.futureExit();
  if (debug) console.debug('app.futureExit() completed');
};

main(
  process.argv.indexOf('--debug') !== -1
)
  .catch((error) => { console.error('error', error); process.exit(1); })
  .then(() => { console.log('completed'); });
