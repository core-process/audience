const net = require('net');
const readline = require('readline');
const path = require('path');

const quotes = require('./quotes');

function getRandomInt(min, max) {
  min = Math.ceil(min);
  max = Math.floor(max);
  return Math.floor(Math.random() * (max - min)) + min;
}

const server = net.createServer(function (peer) {
  const rl = readline.createInterface({ input: peer });
  rl.on('line', (line) => {
    console.log(line);
    const event = JSON.parse(line);
    if (event.name == "window_close_intent") {
      peer.write(JSON.stringify({ "func": "window_destroy", "args": { "handle": event.data.handle } }) + '\n');
    }
    else if (event.name == "window_close") {
      if (event.data.is_last_window) {
        peer.write(JSON.stringify({ "func": "quit" }) + '\n');
      }
    }
    else if (event.name == "window_message") {
      if (event.data.message.trim() == 'quote') {
        peer.write(JSON.stringify({ "func": "window_post_message", "args": { "handle": event.data.handle, "message": quotes[getRandomInt(0, quotes.length)] } }) + '\n');
      }
      else if (event.data.message.trim() == 'quit') {
        peer.write(JSON.stringify({ "func": "quit" }) + '\n');
      }
    }
  });

  peer.write(JSON.stringify({ "func": "window_create", "args": { "dir": path.join(__dirname, '../webapp') } }) + '\n');
});

server.listen(process.argv[2]);
