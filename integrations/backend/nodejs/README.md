# Audience Backend Integration Library

This is the backend integration library of Audience.

You will find further information on [github.com/core-process/audience](https://github.com/core-process/audience).

## What is Audience?
A small adaptive cross-platform webview window library for C/C++ to build modern cross-platform user interfaces.

- It is **small**: The size of the Audience runtime is just a fraction of the size of the Electron runtime. Currently, the ratio is about 1%, further optimizations pending.

- It is **compatible**: Currently, we provide ready-to-go APIs for C/C++ and Node.js, but you can plug it into any environment, which supports either C bindings or can talk to Unix sockets respectively named pipes.

- It is **adaptive**: Audience adapts to its environment using the best available webview technology based on a priority list. E.g., on Windows, it can be configured to try embedding of EdgeHTML first and fall back to the embedding of IE11.

- It supports two-way **messaging**: the web app can post messages to the native backend, and the native backend can post messages to the web app.

- It supports web apps provided via a filesystem folder or URL. Audience provides its custom web server and websocket service tightly integrated into the library. But if you prefer a regular http URL schema, you can use Express or any other http based framework you like.

- The core provides a lightweight C API, a command-line interface, as well as a channel-based API (using Unix sockets and named pipes on Windows). Integrations into other environments are provided on top, e.g., for Node.js.
