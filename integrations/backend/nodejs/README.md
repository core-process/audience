# Audience Backend Integration Library

This is the backend integration library of Audience.

You will find further information on [github.com/core-process/audience](https://github.com/core-process/audience).

## Example

```ts
import { audience, AudienceRect } from 'audience-backend';
import path from 'path';

async function main() {
  // retrieve audience interface
  const app = await audience({
    icons: [16, 32, 48, 64, 96, 512, 1024].map(icon => path.join(__dirname, '../icons', icon + '.png')),
  });

  // handle close related events
  app.onWindowCloseIntent(async ({ handle }) => {
    await app.windowDestroy(handle);
  });

  app.onWindowClose(async ({ is_last_window }) => {
    if (is_last_window) {
      await app.quit();
    }
  });

  // handle window messages
  app.onWindowMessage(async ({ handle, message }) => {
    if (message == 'ping') {
        await app.windowPostMessage(handle, 'pong');
    }
  });

  // create application window
  await app.windowCreate({ dir: path.join(__dirname, '../webapp'), dev: true });

  // wait for future exit of app
  // not required, but nice to manage lifecycle of main()
  await app.futureExit();
};

main()
  .catch((error) => { console.error('error', error); process.exit(1); })
  .then(() => { console.log('completed'); });
```

See [here](https://github.com/core-process/audience/tree/master/examples/terminal) for a complete example.

## What is Audience?
A small adaptive cross-platform and cross-technology webview window library to build modern cross-platform user interfaces.

- It is **small**: The size of the Audience runtime is just a fraction of the size of the Electron runtime. Currently, the ratio is about 1%, further optimizations pending.

- It is **compatible**: Currently, we provide ready-to-go APIs for C/C++ and Node.js, but you can plug it into any environment, which supports either C bindings or can talk to Unix sockets respectively named pipes.

- It is **adaptive**: Audience adapts to its environment using the best available webview technology based on a priority list. E.g., on Windows, it can be configured to try embedding of EdgeHTML first and fall back to the embedding of IE11.

- It supports two-way **messaging**: the web app can post messages to the native backend, and the native backend can post messages to the web app.

- It supports web apps provided via a filesystem folder or URL. Audience provides its custom web server and websocket service tightly integrated into the library. But if you prefer a regular http URL schema, you can use Express or any other http based framework you like.

- The core provides a lightweight C API, a command-line interface, as well as a channel-based API (using Unix sockets and named pipes on Windows). Integrations into other environments are provided on top, e.g., for Node.js.

## Authors and Contributors

Authored and maintained by Niklas Salmoukas [[@GitHub](https://github.com/core-process) [@LinkedIn](https://www.linkedin.com/in/salmoukas/) [@Xing](https://www.xing.com/profile/Niklas_Salmoukas) [@Twitter](https://twitter.com/salmoukas) [@Facebook](https://www.facebook.com/salmoukas) [@Instagram](https://www.instagram.com/salmoukas/)].

[![](https://sourcerer.io/fame/core-process/core-process/audience/images/0)](https://sourcerer.io/fame/core-process/core-process/audience/links/0)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/1)](https://sourcerer.io/fame/core-process/core-process/audience/links/1)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/2)](https://sourcerer.io/fame/core-process/core-process/audience/links/2)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/3)](https://sourcerer.io/fame/core-process/core-process/audience/links/3)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/4)](https://sourcerer.io/fame/core-process/core-process/audience/links/4)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/5)](https://sourcerer.io/fame/core-process/core-process/audience/links/5)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/6)](https://sourcerer.io/fame/core-process/core-process/audience/links/6)[![](https://sourcerer.io/fame/core-process/core-process/audience/images/7)](https://sourcerer.io/fame/core-process/core-process/audience/links/7)
