function fetchWeather(latitude, longitude) {
  var req = new XMLHttpRequest();
  req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
    "lat=" + latitude + "&lon=" + longitude + "&cnt=1", true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if(req.status == 200) {
        var response = JSON.parse(req.responseText);
        var temperature = Math.round(response.main.temp - 273.15);
        Pebble.sendAppMessage({
          "WEATHER_TEMPERATURE_KEY":temperature
        });
      } else {
        console.log("Error");
      }
    }
  };
  req.send(null);
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    "WEATHER_TEMPERATURE_KEY": 255
  });
}

var locationOptions = { "timeout": 15000, "maximumAge": 60000 }; 

Pebble.addEventListener("ready", function(e) {
  locationWatcher = window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
});

Pebble.addEventListener("appmessage", function(e) {
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
});