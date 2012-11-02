var url = chrome.extension.getURL('index.html');
chrome.tabs.create({
  'url': url
});
window.close();
