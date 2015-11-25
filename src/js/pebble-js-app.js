Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready!');
});

Pebble.addEventListener('showConfiguration', function() {
    var url='http://pebble.lastfuture.de/config/arcangle11/';
    console.log('Showing configuration page: '+url);
    Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration page returned: '+JSON.stringify(configData));
    if (configData.colors) {
        Pebble.sendAppMessage({
            colors: configData.colors,
            inverse: 0+(configData.inverse === true)
        }, function() {
            console.log('Send successful!');
        }, function() {
            console.log('Send failed!');
        });
    }
});