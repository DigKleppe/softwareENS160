


// file:///mnt/linuxData/projecten/ENS160meter/softwareENS160/spiffs_image/index.html

const SECONDSPERTICK = (5 * 60);// log interval 
const LOGDAYS = 1;
const MAXDAYPOINTS = (LOGDAYS * 24 * 60 * 60 / SECONDSPERTICK);
const MAXHOURPOINTS = 3600;

const INFOTABLENAME = "sensorInfoTable";
const REQUESTINTERVAL = 1;  // sec
const DAYLOGINTERVAL = (5 * 60); // min

const lineWidth = 3;

var hourChart;
var dayChart;
var lastTimeStamp = 0;
var dayLogPrescaler = 1;
var firstRequest = true;
var firstTime = true; // for table
var averagedValues = [0, 0, 0, 0];
var nrAverages = 0;

//const Utils = ChartUtils.init();

var colours = ['#3366cc', '#dc3912', '#ff9900', '#109618',
    '#990099', '#0099c6', '#dd4477', '#66aa00',
    '#b82e2e', '#316395', '#994499', '#22aa99',
    '#aaaa11', '#6633cc', '#e67300', '#8b0707',
    '#651067', '#329262', '#5574a6', '#3b3eac',
    '#b77322', '#16d620', '#b91383', '#f4359e',
    '#9c5935', '#a9c413', '#2a778d', '#668d1c',
    '#bea413', '#0c5922', '#743411'];


var displayNames = ["AQ1", "ESO2", "Tvoc", "Rs0", "Rs1", "Rs2", "Rs3"];
var cbIDs = ["AQ1cb", "ESO2cb", "Tvoccb", "Rs0cb", "Rs1cb", "Rs2cb", "Rs3cb"];
var cbColours = [colours[0], colours[1], colours[2], colours[3], colours[4], colours[5]];
var chartSeries = [-1, -1, -1, -1, -1, -1, -1];

var NRItems = chartSeries.length;


var options = {
    title: '',
    curveType: 'function',
    legend: { position: 'bottom' },

    heigth: 200,
    crosshair: { trigger: 'both' },	// Display crosshairs on focus and selection.
    explorer: {
        actions: ['dragToZoom', 'rightClickToReset'],
        //actions: ['dragToPan', 'rightClickToReset'],
        axis: 'horizontal',
        keepInBounds: true,
        maxZoomIn: 100.0
    },
    chartArea: { 'width': '90%', 'height': '60%' },

    vAxes: {
        0: { logScale: false },
    },
    /*	series: {
            0: { targetAxisIndex: 0 },// T1
            1: { targetAxisIndex: 0 },// T2
            2: { targetAxisIndex: 0 },// T3
            3: { targetAxisIndex: 0 },// T4
            4: { targetAxisIndex: 0 },// Tref
        },*/
};


function initCharts() {
    var activeSeries = 1;
    hourChart = new google.visualization.LineChart(document.getElementById('hourChart'));
    hourData = new google.visualization.DataTable();
    // dayChart = new google.visualization.LineChart(document.getElementById('dayChart'));
    // dayData = new google.visualization.DataTable();
    hourData.addColumn('string', 'Time');

//   dayData.addColumn('string', 'Time');
    for (var m = 0; m < NRItems; m++) {
        var cb = document.getElementById(cbIDs[m]);
        if (cb) {
            if (cb.checked) {
                hourData.addColumn('number', displayNames[m]);
   //             dayData.addColumn('number', displayNames[m]);
                chartSeries[m] = activeSeries;
                activeSeries++;
            }
        }
    }

    if (SIMULATE)
        //  plotTest();
        simplot();

    timer();
    startTimer();
};

function initChart() {
   window.addEventListener('load', loadCBs());
    window.addEventListener('DOMContentLoaded', initCharts());

};

function clear() {
    hourData.removeRows(0, hourData.getNumberOfRows());
hourChart.draw(hourData, options);
    
//dayData.removeRows(0, dayData.getNumberOfRows());
//    dayChart.draw(dayData, options);
    
}

function clearChart() {
    clear();
}

function clearLog() {
    getItem("clearLog");
    clearChart();
}

function startTimer() {
    if (SIMULATE)
        setInterval(function () { timer() }, 500);
    else
        setInterval(function () { timer() }, REQUESTINTERVAL * 1000);
}
function forgetWifi() {
    getItem("forgetWifi");
}

// plots one sample ( array of 7 items)
function plot(chart, values, timeStamp) {
    var maxPoints;
    if (chart == dayChart) {
        maxPoints = MAXDAYPOINTS;
        data = dayData;
    }
    else {
        maxPoints = MAXHOURPOINTS;
        data = hourData;
    }

    data.addRow();
    var nrRows = data.getNumberOfRows();
    if (nrRows > maxPoints == true)
        data.removeRows(0, nrRows - maxPoints);

    //    console.log("NrRows " + nrRows);

    var arr = values.toString().split(",");
    //if (arr.length >= NRItems) {
    if (arr.length > 0) {
        for (var m = 0; m < NRItems; m++) {
            if (chartSeries[m] != -1) {
                var value = parseFloat(arr[m]); // from string to float
                if (value != '--')
                    data.setValue(nrRows - 1, chartSeries[m], value); // 0 = time 
            }
        }
        var date = new Date(timeStamp);
        var labelText = date.getHours() + ":" + date.getMinutes();//  + ":" + date.getSeconds();
        data.setValue(nrRows - 1, 0, labelText);
    }
}

function plotLog(chart, str) {
    var arr;
    var timeOffset;
    var sampleTime;
    var measTimeLastSample;

    if (chart == dayChart)
        console.log("Log daychart " + str);

    if (str) {
        var arr2 = str.split("\n");

        var nrPoints = arr2.length - 1;
        if (nrPoints > 0) {
            var arr = arr2[nrPoints - 1].split(",");
            measTimeLastSample = arr[0];

            var sec = Date.now();//  / 1000;  // mseconds since 1-1-1970 
            timeOffset = sec - parseFloat(measTimeLastSample) * 1000;

            for (var p = 0; p < nrPoints; p++) {
                arr = arr2[p].split(",");
                if (arr.length >= NRItems) {
                    sampleTime = parseFloat(arr[0]) * 1000 + timeOffset;
                    arr.splice(0, 1); // remove timestamp (arr[0[]) from array
                    plot(chart, arr, sampleTime);
                }
            }
            if (chart == dayChart)
                dayChart.draw(dayData, options);
            else
                hourChart.draw(hourData, options);

            //chart.update();
        }
    }
}

function timer() {
    var arr;
    var str;

    if (SIMULATE) {
        simplot();
    }
    else {
        if (firstRequest) {
            clear();
            // arr = getItem("getDayLogMeasValues");
            // plotLog(dayChart, arr);
            arr = getItem("getHourLogMeasValues");
            plotLog(hourChart, arr);
            firstRequest = false;
        }
        str = getItem("getRTMeasValues");
        if (str) {
            arr = str.split(",");
            if (arr.length >= 3) {
                if (arr[0] > 0) {
                    if (arr[0] != lastTimeStamp) {
                        lastTimeStamp = arr[0];
                        plotLog(hourChart, str);
                        // average values for dayLog
                        var values = str.split(",");
                        for (var m = 0; m < NRItems; m++) {
                            averagedValues[m] += parseFloat(values[m + 1]); // values[0] is timestamp
                        }
                        nrAverages++;

                        // dayLogPrescaler--;
                        // if (dayLogPrescaler == 0) {
                        //     dayLogPrescaler = DAYLOGINTERVAL / REQUESTINTERVAL;
                        //     var str;
                        //     str = arr[0].toString() + ","; // timestamp
                        //     for (var m = 0; m < NRItems; m++) {
                        //         str += (averagedValues[m] / nrAverages).toString();
                        //         if (m < NRItems - 1)
                        //             str += ",";
                        //         else
                        //             str += "\n";
                        //     }
                        //     nrAverages = 0;
                        //     plotLog(dayChart, str);
                        // }
                    }
                }
            }
        }
        else
            console.log("getRTMeasValues failed");

        getInfo("getSensorInfo", INFOTABLENAME, firstTime);
        firstTime = false;
    }
}
function plotTest() {
    var value = [0, 0, 0, 0];
    for (let i = 0; i < 100; ++i) {
        for (let idx = 0; idx < NRItems; idx++) {
            value[idx] = Math.cos(i / 10) + idx;
        }
        var sec = Date.now();//  / 1000;  // mseconds since 1-1-1970 date
        plot(hourChart, value, sec + i * 10000);
        plot(dayChart, value, sec + i * 10000);
    }
    hourChart.update();
    dayChart.update();
}

var presc = 3;
var simVal = 0;
var simTs = 0;

function simplot() {

    var value;
    for (let idx = 0; idx < 100; idx++) {
        value = simTs + ',';

        for (let idx2 = 0; idx2 < NRItems; idx2++) {
            value = value + ((1 + Math.cos(simVal) + idx2).toString() + ',');

        }
        value = value + '\n';
        simTs++;
        simVal += 0.02;
    }
    if (presc-- == 0) {
        plotLog(dayChart, value);
        presc = 3;
    }

    plotLog(hourChart, value);

}

function loadCBs() {
    var cbstate;

    console.log("Reading CBs");

    // Get the current state from localstorage
    // State is stored as a JSON string
    cbstate = JSON.parse(localStorage['CBState'] || '{}');

    // Loop through state array and restore checked 
    // state for matching elements
    for (var i in cbstate) {
        var el = document.querySelector('input[name="' + i + '"]');
        if (el) el.checked = true;
    }

    // Get all checkboxes that you want to monitor state for
    var cb = document.getElementsByClassName('save-cb-state');

    // Loop through results and ...
    for (var i = 0; i < cb.length; i++) {

        //bind click event handler
        cb[i].addEventListener('click', function (evt) {
            // If checkboxe is checked then save to state
            if (this.checked) {
                cbstate[this.name] = true;
            }

            // Else remove from state
            else if (cbstate[this.name]) {
                delete cbstate[this.name];
            }

            // Persist state
            localStorage.CBState = JSON.stringify(cbstate);
        });
    }
    console.log("CBs read");
};