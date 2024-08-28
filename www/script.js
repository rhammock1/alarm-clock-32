document.getElementById('file-upload').addEventListener('submit', function(event) {
  event.preventDefault();
  const fileInput = document.getElementById('files-input');
  const {files} = fileInput;
  console.log('event target', event.target, 'files', files);
  if (files.length === 0) {
    return;
  }
  for(const file of files) {
    const formData = new FormData();
    formData.append('file', file);
    formData.append('overwrite_html', document.getElementById('overwrite_html').checked);
    fetch('/file', {
      method: 'POST',
      body: formData
    })
    .then(response => response.ok && response.text())
    .then(result => {
      console.log('Success:', result);
    })
    .catch(error => {
      console.error('Error:', error);
    });
  }
});

document.getElementById('format-fs').addEventListener('click', function() {
  fetch('/format')
  .then(response => response.ok && response.text())
  .then(result => {
    console.log('Success:', result);
  })
  .catch(error => {
    console.error('Error:', error);
  });
});

document.getElementById('set-time').addEventListener('click', function() {
  const now = new Date();
  const timezoneOffset = now.getTimezoneOffset();
  const offsetMillis = timezoneOffset * 60 * 1000;
  const localTime = new Date(now.getTime() - offsetMillis);
  fetch('/time', {
    method: 'POST',
    body: JSON.stringify(localTime),
  })
  .then(response => response.ok && response.text())
  .then(result => {
    console.log('Success:', result);
  })
  .catch(error => {
    console.error('Error:', error);
  });
});

document.getElementById('get-files').addEventListener('click', function() {
  fetch('/files')
    .then(response => response.ok && response.json())
    .then(result => {
      console.log('Success:', result);
    })
    .catch(error => {
      console.error('Error:', error);
    });
});

document.getElementById('play-sound').addEventListener('click', function() {
  fetch('/sound')
    .then(response => response.ok && response.text())
    .then(result => {
      console.log('Success:', result);
    })
    .catch(error => {
      console.error('Error:', error);
    });
});
