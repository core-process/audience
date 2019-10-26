(function () {
  // prevent double initialization
  if (window.audience !== undefined)
    return;

  // queues to handle init states properly
  var qin = [];
  var qout = [];

  // message handler
  var handlers = new Set();

  // push routines
  function pushOutgoing() {
    while (window.external !== undefined && qout.length > 0) {
      try {
        window.external.notify(qout[0]);
        qout.shift();
      }
      catch (error) {
        console.error(error);
        break;
      }
    }
  }

  function pushIncoming() {
    while (handlers.size > 0 && qin.length > 0) {
      try {
        handlers.forEach(function (handler) {
          handler(qin[0]);
        });
        qin.shift();
      }
      catch (error) {
        console.error(error);
        break;
      }
    }
  }

  // public interface
  window.audience = {};

  window._audienceIncomingMessage = function (message) {
    qin.push(message);
    pushIncoming();
  };

  window.audience.postMessage = function (message) {
    // currently we support strings only
    if (typeof message != 'string')
      throw new Error('only string messages are supported');
    // put in queue and trigger send
    qout.push(message);
    pushOutgoing();
  };

  window.audience.onMessage = function (handler) {
    handlers.add(handler);
  };

  window.audience.offMessage = function (handler) {
    if (handler !== undefined) {
      handlers.delete(handler);
    }
    else {
      handlers.clear();
    }
  };
})();
