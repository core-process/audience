import net from 'net';
import os from 'os';
import readline from 'readline';
import child_process from 'child_process';
import path from 'path';

export type AudienceWindowHandle = number;

export type AudienceSize = {
  width: number;
  height: number;
};

export type AudienceRect = AudienceSize & {
  x: number;
  y: number;
};

export type AudienceScreenList = {
  focused: number;
  primary: number;
  screens: {
    frame: AudienceRect;
    workspace: AudienceRect;
  }[];
};

export type AudienceWindowList = {
  focused: number;
  windows: {
    handle: AudienceWindowHandle;
    frame: AudienceRect;
    workspace: AudienceSize;
  }[];
};

export type AudienceWindowDetails = {
  dir?: string;
  url?: string;
  title?: string;
  size?: [number, number];
  pos?: [number, number];
  decorated?: boolean;
  resizable?: boolean;
  top?: boolean;
  dev?: boolean;
};

type _EventCallbackWindowMessage = (data: { handle: AudienceWindowHandle, message: string }) => void;
type _EventCallbackWindowCloseIntent = (data: { handle: AudienceWindowHandle }) => void;
type _EventCallbackWindowClose = (data: { handle: AudienceWindowHandle, is_last_window: boolean }) => void;
type _EventCallbackAppQuit = () => void;

type _EventCallbackAny =
  _EventCallbackWindowMessage |
  _EventCallbackWindowCloseIntent |
  _EventCallbackWindowClose |
  _EventCallbackAppQuit;

export interface AudienceApi {
  // Commands
  screenList(): Promise<AudienceScreenList>;
  windowList(): Promise<AudienceWindowList>;
  windowCreate(details: AudienceWindowDetails): Promise<AudienceWindowHandle>;
  windowUpdatePosition(handle: AudienceWindowHandle, position: AudienceRect): Promise<void>;
  windowPostMessage(handle: AudienceWindowHandle, message: string): Promise<void>;
  windowDestroy(handle: AudienceWindowHandle): Promise<void>;
  quit(): Promise<void>;
  // Events
  onWindowMessage(callback: _EventCallbackWindowMessage): void;
  onWindowCloseIntent(callback: _EventCallbackWindowCloseIntent): void;
  onWindowClose(callback: _EventCallbackWindowClose): void;
  onAppQuit(callback: _EventCallbackAppQuit): void;
  off(callback?: _EventCallbackAny): void;
  futureExit(): Promise<void>;
};

export type AudienceOptions = {
  win?: string[],
  mac?: string[],
  unix?: string[],
  icons?: string[],
  debug?: boolean,
};

export async function audience(options?: AudienceOptions): Promise<AudienceApi> {

  const activeCommands = new Map<string, { reject: (error: Error) => void, resolve: (result?: any) => void }>();
  const eventHandler = new Map<'window_message' | 'window_close_intent' | 'window_close' | 'app_quit', Set<_EventCallbackAny>>([
    ['window_message', new Set<_EventCallbackAny>()],
    ['window_close_intent', new Set<_EventCallbackAny>()],
    ['window_close', new Set<_EventCallbackAny>()],
    ['app_quit', new Set<_EventCallbackAny>()],
  ]);

  // event processing machinery
  function processEvent({ name, data }: { name: string, data: any }) {
    if (name == 'command_succeeded' || name == 'command_failed') {
      const promise = activeCommands.get(data.id);
      if (promise) {
        activeCommands.delete(data.id);
        if (name == 'command_succeeded') {
          promise.resolve(data.result);
        }
        else {
          promise.reject(new Error('command failed'));
        }
      }
    }
    else if (eventHandler.has(<any>name)) {
      eventHandler.get(<any>name)!.forEach((callback) => {
        callback(data);
      });
    }
  }

  // start rpc server
  const socketPath = (os.platform() == 'win32' ? '\\\\.\\pipe\\' : '/tmp/') + `audience_${Date.now()}_${process.pid}`;
  const server = net.createServer();
  const futurePeer = new Promise<net.Socket>((resolve) => {
    server.once('connection', (peer: net.Socket) => {
      const incoming = readline.createInterface({ input: peer });
      incoming.on('line', (line) => {
        processEvent(JSON.parse(line));
      });
      resolve(peer);
    });
  });
  server.listen(socketPath);

  // start audience process
  const audienceProcess = child_process.spawn(
    path.join(process.env.AUDIENCE_RUNTIME_DIR || __dirname, 'audience' + (os.platform() == 'win32' ? '.exe' : '')),
    [
      '--channel', socketPath,
      ...(options && options.win ? ['--win', options.win.join(',')] : []),
      ...(options && options.mac ? ['--mac', options.mac.join(',')] : []),
      ...(options && options.unix ? ['--unix', options.unix.join(',')] : []),
      ...(options && options.icons ? ['--icons', options.icons.join(',')] : []),
    ]
  );
  const futureExit = new Promise<void>((resolve) => {
    audienceProcess.on('exit', () => {
      server.close();
      resolve();
    });
  });
  if(options && options.debug) {
    audienceProcess.stdout.setEncoding('utf8');
    audienceProcess.stderr.setEncoding('utf8');
    readline.createInterface({ input: audienceProcess.stdout }).on('line', (line) => { console.log(line); });
    readline.createInterface({ input: audienceProcess.stderr }).on('line', (line) => { console.error(line); });
  }

  // command execution machinery
  const peer = await futurePeer;
  function dispatchCommand(func: string, args?: any): Promise<any> {
    // find free command id
    let idNo = Date.now();
    while (activeCommands.has(idNo.toString())) {
      idNo += Math.round(Math.random() * 100);
    }
    const id = idNo.toString();
    // setup promise
    return new Promise((resolve, reject) => {
      activeCommands.set(id, { resolve, reject });
      peer.write(JSON.stringify({ id, func, args }) + '\n');
    });
  };

  return {
    // Commands
    screenList(): Promise<AudienceScreenList> {
      return dispatchCommand('screen_list');
    },
    windowList(): Promise<AudienceWindowList> {
      return dispatchCommand('window_list');
    },
    windowCreate(details: AudienceWindowDetails): Promise<AudienceWindowHandle> {
      return dispatchCommand('window_create', details);
    },
    windowUpdatePosition(handle: AudienceWindowHandle, position: AudienceRect): Promise<void> {
      return dispatchCommand('window_update_position', { handle, ...position });
    },
    windowPostMessage(handle: AudienceWindowHandle, message: string): Promise<void> {
      return dispatchCommand('window_post_message', { handle, message });
    },
    windowDestroy(handle: AudienceWindowHandle): Promise<void> {
      return dispatchCommand('window_destroy', { handle });
    },
    quit(): Promise<void> {
      return dispatchCommand('quit');
    },
    // Events
    onWindowMessage(callback: _EventCallbackWindowMessage): void {
      eventHandler.get('window_message')!.add(callback);
    },
    onWindowCloseIntent(callback: _EventCallbackWindowCloseIntent): void {
      eventHandler.get('window_close_intent')!.add(callback);
    },
    onWindowClose(callback: _EventCallbackWindowClose): void {
      eventHandler.get('window_close')!.add(callback);
    },
    onAppQuit(callback: _EventCallbackAppQuit): void {
      eventHandler.get('app_quit')!.add(callback);
    },
    off(callback?: _EventCallbackAny): void {
      eventHandler.forEach((callbacks) => {
        if (callback) {
          callbacks.delete(callback);
        }
        else {
          callbacks.clear();
        }
      });
    },
    futureExit(): Promise<void> {
      return futureExit;
    },
  };
}
