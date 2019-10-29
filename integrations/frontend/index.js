"use strict";
if (window.audience !== undefined)
    throw new Error('double initialization of audience frontend detected');
var WebsocketBackend = (function () {
    function class_1(readyHandler, messageHandler) {
        var _this = this;
        this.readyHandler = readyHandler;
        this.messageHandler = messageHandler;
        this.ws = new WebSocket('ws' + window.location.origin.substr(4));
        this.ws.addEventListener('open', function () {
            _this.readyHandler();
        });
        this.ws.addEventListener('message', function (event) {
            _this.messageHandler(event.data);
        });
    }
    class_1.prototype.ready = function () {
        return this.ws.readyState == 1;
    };
    class_1.prototype.send = function (message) {
        this.ws.send(message);
    };
    return class_1;
}());
var EdgeWebviewBackend = (function () {
    function class_2(readyHandler, messageHandler) {
        var _this = this;
        this.readyHandler = readyHandler;
        this.messageHandler = messageHandler;
        setTimeout(function () {
            _this.readyHandler();
        });
        window._audienceWebviewMessageHandler = function (message) {
            _this.messageHandler(message);
        };
    }
    class_2.prototype.ready = function () {
        return window.external !== undefined;
    };
    class_2.prototype.send = function (message) {
        window.external.notify(message);
    };
    return class_2;
}());
var backend;
var queues = {
    in: [],
    out: []
};
var handlers = new Set();
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
function backendReadyHandler() {
    pushToBackend();
}
function backendMessageHandler(message) {
    if (typeof message != 'string')
        throw new Error('only string messages are supported');
    queues.in.push(message);
    pushToHandlers();
}
backend = window._audienceWebviewSignature == 'edge'
    ? new EdgeWebviewBackend(backendReadyHandler, backendMessageHandler)
    : new WebsocketBackend(backendReadyHandler, backendMessageHandler);
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
