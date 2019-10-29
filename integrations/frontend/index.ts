interface Window {
  audience: {
    postMessage: (message: string) => void;
    onMessage: (handler: (message: string) => void) => void;
    offMessage: (handler: ((message: string) => void) | undefined) => void;
  },
  _audienceWebviewSignature?: 'edge';
  _audienceWebviewMessageHandler?: (message: string) => void;
}

interface External {
  notify?: (message: string) => void;
}

interface BackendConstructor {
  new(
    readyHandler: () => void,
    messageHandler: (message: string) => void
  ): Backend;
}

interface Backend {
  ready: () => boolean;
  send: (message: string) => void;
}

// prevent double initialization
if (window.audience !== undefined)
  throw new Error('double initialization of audience frontend detected');

// backend implementations
const WebsocketBackend: BackendConstructor = class implements Backend {
  private ws = new WebSocket('ws' + window.location.origin.substr(4));
  constructor(
    private readyHandler: () => void,
    private messageHandler: (message: string) => void
  ) {
    this.ws.addEventListener('open', () => {
      this.readyHandler();
    });
    this.ws.addEventListener('message', (event: { data: string }) => {
      this.messageHandler(event.data);
    });
  }
  ready() {
    return this.ws.readyState == 1;
  }
  send(message: string) {
    this.ws.send(message);
  }
};

const EdgeWebviewBackend: BackendConstructor = class implements Backend {
  constructor(
    private readyHandler: () => void,
    private messageHandler: (message: string) => void
  ) {
    setTimeout(() => {
      this.readyHandler();
    });
    window._audienceWebviewMessageHandler = (message: string) => {
      this.messageHandler(message);
    };
  }
  ready() {
    return window.external !== undefined;
  }
  send(message: string) {
    // IMPORTANT: DO NOT CHECK IF window.external.notify IS undefined.
    //            IT IS undefined!, EVEN THOUGH YOU CAN STILL CALL IT.
    //            VERY WEIRD MICROSOFT STUFF!
    // lets perform a typecast to make typescript happy...
    (<(message: string) => void>window.external.notify)(message);
  }
};

// message queues and handlers
let backend: Backend;

const queues = {
  in: <Array<string>>[],
  out: <Array<string>>[]
};

const handlers: Set<(message: string) => void> = new Set();

function pushToBackend() {
  while (backend.ready() && queues.out.length > 0) {
    try {
      backend.send(queues.out[0]);
      queues.out.shift();
    }
    catch (error) {
      console.error(error);
      break;
    }
  }
}

function pushToHandlers() {
  while (handlers.size > 0 && queues.in.length > 0) {
    try {
      handlers.forEach(function (handler) {
        handler(queues.in[0]);
      });
      queues.in.shift();
    }
    catch (error) {
      console.error(error);
      break;
    }
  }
}

// instatiate backend
function backendReadyHandler() {
  pushToBackend();
}

function backendMessageHandler(message: string) {
  if (typeof message != 'string')
    throw new Error('only string messages are supported');
  queues.in.push(message);
  pushToHandlers();
}

backend = window._audienceWebviewSignature == 'edge'
  ? new EdgeWebviewBackend(backendReadyHandler, backendMessageHandler)
  : new WebsocketBackend(backendReadyHandler, backendMessageHandler);

// register public interface
window.audience = {
  postMessage: function (message) {
    if (typeof message != 'string')
      throw new Error('only string messages are supported');
    queues.out.push(message);
    pushToBackend();
  },
  onMessage: function (handler) {
    handlers.add(handler);
  },
  offMessage: function (handler) {
    if (handler !== undefined) {
      handlers.delete(handler);
    }
    else {
      handlers.clear();
    }
  }
};
