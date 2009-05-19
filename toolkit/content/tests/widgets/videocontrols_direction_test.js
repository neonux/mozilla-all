var RemoteCanvas = function(url, id) {
  this.url = url;
  this.id = id;
  this.snapshot = null;
};

RemoteCanvas.CANVAS_WIDTH = 200;
RemoteCanvas.CANVAS_HEIGHT = 200;

RemoteCanvas.prototype.compare = function(otherCanvas, expected) {
  return compareSnapshots(this.snapshot, otherCanvas.snapshot, expected)[0];
}

RemoteCanvas.prototype.load = function(callback) {
  var iframe = document.createElement("iframe");
  iframe.id = this.id + "-iframe";
  iframe.width = RemoteCanvas.CANVAS_WIDTH + "px";
  iframe.height = RemoteCanvas.CANVAS_HEIGHT + "px";
  iframe.src = this.url;
  var me = this;
  iframe.addEventListener("load", function() {
    me.remotePageLoaded(callback);
  }, false);
  window.document.body.appendChild(iframe);
};

RemoteCanvas.prototype.remotePageLoaded = function(callback) {
  var ldrFrame = document.getElementById(this.id + "-iframe");
  this.snapshot = snapshotWindow(ldrFrame.contentWindow);
  this.snapshot.id = this.id + "-canvas";
  window.document.body.appendChild(this.snapshot);
  callback(this);
};

RemoteCanvas.prototype.cleanup = function() {
  var iframe = document.getElementById(this.id + "-iframe");
  iframe.parentNode.removeChild(iframe);
  var canvas = document.getElementById(this.id + "-canvas");
  canvas.parentNode.removeChild(canvas);
};

function runTest(index) {
  var canvases = [];
  function testCallback(canvas) {
    canvases.push(canvas);

    if (canvases.length == 2) { // when both canvases are loaded
      var expectedEqual = currentTest.op == "==";
      var result = canvases[0].compare(canvases[1], expectedEqual);
      ok(result, "Rendering of reftest " + currentTest.test + " should " +
        (expectedEqual ? "not " : "") + "be different to the reference");

      if (result) {
        canvases[0].cleanup();
        canvases[1].cleanup();
      }

      if (index < tests.length - 1)
        runTest(index + 1);
      else
        SimpleTest.finish();
    }
  }

  var currentTest = tests[index];
  var testCanvas = new RemoteCanvas(currentTest.test, "test-" + index);
  testCanvas.load(testCallback);

  var refCanvas = new RemoteCanvas(currentTest.ref, "ref-" + index);
  refCanvas.load(testCallback);
}

SimpleTest.waitForExplicitFinish();

window.addEventListener("load", function() { runTest(0); }, true);
