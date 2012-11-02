var CLIENT_ID = "412923012522.apps.googleusercontent.com";
var SCOPES = "https://www.googleapis.com/auth/drive.readonly";

var queued = [];
var db;
var jobPending = false;

function handleClientLoad() {
  gapi.client.setApiKey("AIzaSyAeecvsDgmnmURPNuZlM_n5WhVi47p0aTw");
  window.setTimeout(checkAuth, 1);
}

function checkAuth() {
  gapi.auth.authorize(
      {'client_id': CLIENT_ID, 'scope': SCOPES, 'immediate': true},
      handleAuthResult);
}

function handleAuthResult(authResult) {
  if (authResult && !authResult.error) {
    fetchItems();
  }
}

function fetchItems(pageToken) {
  params = {'q': 'mimeType="audio/mpeg" and trashed = false'};
  if (pageToken) {
    params['pageToken'] = pageToken;
  }
  var request = gapi.client.request({
      'path': '/drive/v2/files',
      'method': 'GET',
      'params': params});
  request.execute(function(response) {
    console.log(response);
    var items = response['items'];
    for (var i = 0; i < items.length; ++i) {
      index(items[i]);
    }
    popJob();
    if (response['nextPageToken']) {
      fetchItems(response['nextPageToken']);
    }
  });
}

function index(item) {
  var downloadUrl = item['downloadUrl']
  downloadUrl += '&access_token=' + gapi.auth.getToken().access_token;
  var message = {
    id: item['id'],
    url: downloadUrl,
    length: Number(item['fileSize']),
    filename: item['originalFilename']
  };
  queueJob(message);
}

function queueJob(message) {
  queued.push(message);
}

function popJob() {
  var job = queued.shift();
  if (job) {
    var transaction = db.transaction(['songs'], "readonly");
    var store = transaction.objectStore('songs');
    console.log("Counting:" + job['id']);
    db.transaction(['songs'], "readonly")
        .objectStore('songs')
        .count(job['id']).onsuccess = function(e) {
      if (this.result) {
        console.log('Already indexed:' + job['id']);
        popJob();
      } else {
        if (job['filename'].match(/\.mp3$/)) {
          if (!jobPending) {
            sendJob(job);
          } else {
            queued.unshift(job);
          }
        } else {
          console.log("Skipping non-mp3:" + job['filename']);
          popJob();
        }
      }
    };
  }
}

function sendJob(job) {
  console.log("NaCl job starting");
  jobPending = true;
  var module = document.getElementById('nacl');
  console.log(JSON.stringify(job));
  module.postMessage(JSON.stringify(job));
}

function authorize() {
  gapi.auth.authorize(
      {'client_id': CLIENT_ID, 'scope': SCOPES, 'immediate': false},
      handleAuthResult);
}

function moduleDidLoad() {
  var module = document.getElementById('nacl');
}

function getAllSongs() {
  db.transaction(['songs'])
      .objectStore('songs')
      .openCursor().onsuccess = function(e) {
    var cursor = this.result;
    if (cursor) {
      var song = cursor.value;
      var div = document.createElement('div');
      div.innerText = song['title'] + ' - ' + song['artist'] + ' - ' + song['album'];
      var button = document.createElement('button');
      button.innerText = "play";
      button.onclick = function() {
        play(song);
      };
      div.appendChild(button);
      document.getElementById('songs').appendChild(div);
      cursor.continue();
    }
  };
}

function play(song) {
  var request = gapi.client.request({
      'path': '/drive/v2/files/' + song['id'],
      'method': 'GET'});
  request.execute(function(response) {
    console.log(response);
    var url = response['downloadUrl'];
    url += '&access_token=' + gapi.auth.getToken().access_token;
    var player = document.getElementById('player');
    player.src = url;
    player.play();
  });
}

function handleMessage(message_event) {
  jobPending = false;
  console.log('NaCl job ending');
  console.log(message_event.data);
  var data = JSON.parse(message_event.data);
  if (data['id']) {
    console.log("Tagged:" + data['id']);
    if (!data['title']) {
      data['title'] = data['filename'];
    }
    addSong(data);
  }
  popJob();
}

function addSong(tag) {
  var transaction = db.transaction(['songs'], "readwrite");
  var store = transaction.objectStore('songs');
  store.put(tag);

  /*
  var audio = document.createElement('audio');
  audio.src = tag['url'];
  audio.controls = true;
  audio.preload = false;

  var song = document.createElement('div');
  song.innerText = title + ' - ' + album + ' - ' + artist;
  song.appendChild(audio);

  var songs = document.getElementById('songs');
  songs.appendChild(song);
  */
}

document.addEventListener('DOMContentLoaded', function() {
  var listener = document.getElementById('listener');
  listener.addEventListener('onload', moduleDidLoad, true);
  listener.addEventListener('message', handleMessage, true);

  var button = document.getElementById('authorizeButton');
  button.addEventListener('click', authorize);

  var request = webkitIndexedDB.open('clementine');
  console.log('Opening DB');
  request.onsuccess = function(event) {
    db = this.result;
    var version_request = db.setVersion("2.0")
    console.log('Version request');
    version_request.onsuccess = function(e) {
      console.log('Successful version request');
      store = db.createObjectStore('songs', {keyPath: 'id'});
    };
  };
});
