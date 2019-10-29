interface Window {
    audience: {
        postMessage: (message: string) => void;
        onMessage: (handler: (message: string) => void) => void;
        offMessage: (handler: ((message: string) => void) | undefined) => void;
    };
    _audienceWebviewSignature?: 'edge';
    _audienceWebviewMessageHandler?: (message: string) => void;
}
interface External {
    notify?: (message: string) => void;
}
interface BackendConstructor {
    new (readyHandler: () => void, messageHandler: (message: string) => void): Backend;
}
interface Backend {
    ready: () => boolean;
    send: (message: string) => void;
}
declare const WebsocketBackend: BackendConstructor;
declare const EdgeWebviewBackend: BackendConstructor;
declare let backend: Backend;
declare const queues: {
    in: string[];
    out: string[];
};
declare const handlers: Set<(message: string) => void>;
declare function pushToBackend(): void;
declare function pushToHandlers(): void;
declare function backendReadyHandler(): void;
declare function backendMessageHandler(message: string): void;
