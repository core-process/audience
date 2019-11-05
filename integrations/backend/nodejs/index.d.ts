export declare type AudienceWindowHandle = number;
export declare type AudienceSize = {
    width: number;
    height: number;
};
export declare type AudienceRect = AudienceSize & {
    x: number;
    y: number;
};
export declare type AudienceScreenList = {
    focused: number;
    primary: number;
    screens: {
        frame: AudienceRect;
        workspace: AudienceRect;
    }[];
};
export declare type AudienceWindowList = {
    focused: number;
    windows: {
        handle: AudienceWindowHandle;
        frame: AudienceRect;
        workspace: AudienceSize;
    }[];
};
export declare type AudienceWindowDetails = {
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
declare type _EventCallbackWindowMessage = (data: {
    handle: AudienceWindowHandle;
    message: string;
}) => void;
declare type _EventCallbackWindowCloseIntent = (data: {
    handle: AudienceWindowHandle;
}) => void;
declare type _EventCallbackWindowClose = (data: {
    handle: AudienceWindowHandle;
    is_last_window: boolean;
}) => void;
declare type _EventCallbackAppQuit = () => void;
declare type _EventCallbackAny = _EventCallbackWindowMessage | _EventCallbackWindowCloseIntent | _EventCallbackWindowClose | _EventCallbackAppQuit;
export interface AudienceApi {
    screenList(): Promise<AudienceScreenList>;
    windowList(): Promise<AudienceWindowList>;
    windowCreate(details: AudienceWindowDetails): Promise<AudienceWindowHandle>;
    windowUpdatePosition(handle: AudienceWindowHandle, position: AudienceRect): Promise<void>;
    windowPostMessage(handle: AudienceWindowHandle, message: string): Promise<void>;
    windowDestroy(handle: AudienceWindowHandle): Promise<void>;
    quit(): Promise<void>;
    onWindowMessage(callback: _EventCallbackWindowMessage): void;
    onWindowCloseIntent(callback: _EventCallbackWindowCloseIntent): void;
    onWindowClose(callback: _EventCallbackWindowClose): void;
    onAppQuit(callback: _EventCallbackAppQuit): void;
    off(callback?: _EventCallbackAny): void;
    futureExit(): Promise<void>;
}
export declare type AudienceOptions = {
    win?: string[];
    mac?: string[];
    unix?: string[];
    icons?: string[];
    debug?: boolean;
};
export declare function audience(options?: AudienceOptions): Promise<AudienceApi>;
export {};
