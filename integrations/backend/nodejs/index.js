"use strict";
var __assign = (this && this.__assign) || function () {
    __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
                t[p] = s[p];
        }
        return t;
    };
    return __assign.apply(this, arguments);
};
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
var __spreadArrays = (this && this.__spreadArrays) || function () {
    for (var s = 0, i = 0, il = arguments.length; i < il; i++) s += arguments[i].length;
    for (var r = Array(s), k = 0, i = 0; i < il; i++)
        for (var a = arguments[i], j = 0, jl = a.length; j < jl; j++, k++)
            r[k] = a[j];
    return r;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
var net_1 = __importDefault(require("net"));
var os_1 = __importDefault(require("os"));
var readline_1 = __importDefault(require("readline"));
var child_process_1 = __importDefault(require("child_process"));
var path_1 = __importDefault(require("path"));
;
function audience(options) {
    return __awaiter(this, void 0, void 0, function () {
        function processEvent(_a) {
            var name = _a.name, data = _a.data;
            if (name == 'command_succeeded' || name == 'command_failed') {
                var promise = activeCommands.get(data.id);
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
            else if (eventHandler.has(name)) {
                eventHandler.get(name).forEach(function (callback) {
                    callback(data);
                });
            }
        }
        function dispatchCommand(func, args) {
            var idNo = Date.now();
            while (activeCommands.has(idNo.toString())) {
                idNo += Math.round(Math.random() * 100);
            }
            var id = idNo.toString();
            return new Promise(function (resolve, reject) {
                activeCommands.set(id, { resolve: resolve, reject: reject });
                peer.write(JSON.stringify({ id: id, func: func, args: args }) + '\n');
            });
        }
        var activeCommands, eventHandler, socketPath, server, futurePeer, audienceProcess, futureExit, peer;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    activeCommands = new Map();
                    eventHandler = new Map([
                        ['window_message', new Set()],
                        ['window_close_intent', new Set()],
                        ['window_close', new Set()],
                        ['app_quit', new Set()],
                    ]);
                    socketPath = (os_1.default.platform() == 'win32' ? '\\\\.\\pipe\\' : '/tmp/') + ("audience_" + Date.now() + "_" + process.pid);
                    server = net_1.default.createServer();
                    futurePeer = new Promise(function (resolve) {
                        server.once('connection', function (peer) {
                            var incoming = readline_1.default.createInterface({ input: peer });
                            incoming.on('line', function (line) {
                                processEvent(JSON.parse(line));
                            });
                            resolve(peer);
                        });
                    });
                    server.listen(socketPath);
                    audienceProcess = child_process_1.default.spawn(path_1.default.join(process.env.AUDIENCE_RUNTIME_DIR || __dirname, 'audience' + (os_1.default.platform() == 'win32' ? '.exe' : '')), __spreadArrays([
                        '--channel', socketPath
                    ], (options && options.win ? ['--win', options.win.join(',')] : []), (options && options.mac ? ['--mac', options.mac.join(',')] : []), (options && options.unix ? ['--unix', options.unix.join(',')] : []), (options && options.icons ? ['--icons', options.icons.join(',')] : [])));
                    futureExit = new Promise(function (resolve) {
                        audienceProcess.on('exit', function () {
                            server.close();
                            resolve();
                        });
                    });
                    if (options && options.debug) {
                        audienceProcess.stdout.setEncoding('utf8');
                        audienceProcess.stderr.setEncoding('utf8');
                        readline_1.default.createInterface({ input: audienceProcess.stdout }).on('line', function (line) { console.log(line); });
                        readline_1.default.createInterface({ input: audienceProcess.stderr }).on('line', function (line) { console.error(line); });
                    }
                    return [4, futurePeer];
                case 1:
                    peer = _a.sent();
                    ;
                    return [2, {
                            screenList: function () {
                                return dispatchCommand('screen_list');
                            },
                            windowList: function () {
                                return dispatchCommand('window_list');
                            },
                            windowCreate: function (details) {
                                return dispatchCommand('window_create', details);
                            },
                            windowUpdatePosition: function (handle, position) {
                                return dispatchCommand('window_update_position', __assign({ handle: handle }, position));
                            },
                            windowPostMessage: function (handle, message) {
                                return dispatchCommand('window_post_message', { handle: handle, message: message });
                            },
                            windowDestroy: function (handle) {
                                return dispatchCommand('window_destroy', { handle: handle });
                            },
                            quit: function () {
                                return dispatchCommand('quit');
                            },
                            onWindowMessage: function (callback) {
                                eventHandler.get('window_message').add(callback);
                            },
                            onWindowCloseIntent: function (callback) {
                                eventHandler.get('window_close_intent').add(callback);
                            },
                            onWindowClose: function (callback) {
                                eventHandler.get('window_close').add(callback);
                            },
                            onAppQuit: function (callback) {
                                eventHandler.get('app_quit').add(callback);
                            },
                            off: function (callback) {
                                eventHandler.forEach(function (callbacks) {
                                    if (callback) {
                                        callbacks.delete(callback);
                                    }
                                    else {
                                        callbacks.clear();
                                    }
                                });
                            },
                            futureExit: function () {
                                return futureExit;
                            },
                        }];
            }
        });
    });
}
exports.audience = audience;
