import 'audience-frontend';
import { Chart } from 'chart.js';
import { Notyf } from 'notyf';
import 'notyf/notyf.min.css';

var notyf = new Notyf();

var ctx = document.getElementById('chart');
var chart = new Chart(ctx, {
  type: 'line',
  data: {
    datasets: [{
      label: 'ping 8.8.8.8',
      backgroundColor: 'rgb(54, 162, 235)',
      borderColor: 'rgb(54, 162, 235)',
      data: [],
      pointRadius: 0,
    }],
  },
  options: {
    maintainAspectRatio: false,
    scales: {
      xAxes: [{
        type: 'time',
        distribution: 'linear',
        ticks: {
          source: 'auto',
        },
        time: {
          unit: 'second',
          min: Date.now() - 30 * 1000,
          max: Date.now() + 0 * 1000,
        },
      }],
      yAxes: [{
        display: true,
        scaleLabel: {
          display: true,
          labelString: 'ms'
        }
      }]
    }
  }
});

window.audience.onMessage(function (message) {
  message = JSON.parse(message);
  if (message.timestamp !== undefined && message.roundtrip !== undefined) {
    chart.options.scales.xAxes[0].time.min = message.timestamp - 30 * 1000;
    chart.options.scales.xAxes[0].time.max = message.timestamp + 0 * 1000;
    chart.data.datasets[0].data.push({
      x: new Date(message.timestamp),
      y: message.roundtrip
    });
    chart.update();
  }
  if (message.error) {
    notyf.error(message.error);
  }
});

document.addEventListener('keydown', function (event) {
  if (event.key === "Escape" || event.key === "Esc") {
    window.audience.postMessage('close');
  }
});

notyf.success('ESC to close');
