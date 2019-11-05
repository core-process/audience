const audience = require('../../../integrations/backend/nodejs').audience;
const quotes = require('./quotes');
const path = require('path');

async function main(debug) {
  // retrieve audience interface
  const app = await audience({
    icons: [
      path.join(__dirname, '../icons/16.png'),
      path.join(__dirname, '../icons/32.png'),
      path.join(__dirname, '../icons/64.png'),
      path.join(__dirname, '../icons/128.png'),
      path.join(__dirname, '../icons/512.png'),
    ],
    debug,
  });

  // handle window messages
  app.onWindowMessage(({ handle, message }) => {
    if (message == 'quote') {
      app.windowPostMessage(handle, quotes[Math.floor(Math.random() * quotes.length)]);
    }
    else if (message == 'quit') {
      app.quit();
    }
  });

  // create application window
  app.windowCreate({ dir: path.join(__dirname, '../webapp') });

  // wait for future exit of app
  // not required, but nice to manage lifecycle of main()
  await app.futureExit();
};

main(
  process.argv.indexOf('--debug') !== -1
)
  .catch((error) => { console.error('error', error); process.exit(1); })
  .then(() => { console.log('completed'); });
