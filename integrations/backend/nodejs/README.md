# Audience Backend Integration Library

This is the backend integration library of Audience.

You will find further information on [github.com/core-process/audience](https://github.com/core-process/audience).

## What is Audience?
A small adaptive cross-platform webview window library for C/C++ to build modern cross-platform user interfaces.

- It is **adaptive**: Audience adapts to its environment using the best available webview technology based on a priority list. E.g., on Windows, it can be configured to try embedding of EdgeHTML first and fall back to the embedding of IE11.

- It supports two-way **messaging**: the web app can post messages to the native backend, and the native backend can post messages to the web app.

- It supports web apps provided via a filesystem folder or URL. Audience provides its custom web server and websocket service tightly integrated into the library. But if you prefer a regular http URL schema, you can use Express or any other http based framework you like.
