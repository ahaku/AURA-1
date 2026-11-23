import assert from "node:assert/strict";

// Starting a recording must neither change the LED reading of a fixed PCM
// signal nor merge the stereo channels.
export async function checkRecordingMeter(page) {
  const readings = await page.evaluate(async () => {
    const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
    const originals = [window.audioAnalyser, window.audioAnalyserL, window.audioAnalyserR];
    const analyser = (amplitude) => ({
      fftSize: 256,
      getFloatTimeDomainData(buffer) {
        for (let i = 0; i < buffer.length; i++) buffer[i] = amplitude * Math.sin(i * Math.PI / 16);
      },
    });
    const readLED = () => {
      const canvas = document.getElementById("tapeLedCanvas");
      const context = canvas.getContext("2d");
      const scale = canvas.width / 122;
      return [7, 17].map((y) => {
        let lit = 0;
        for (let i = 0; i < 10; i++) {
          const pixel = context.getImageData(Math.round((19 + i * 10) * scale), Math.round(y * scale), 1, 1).data;
          if (pixel[0] + pixel[1] + pixel[2] > 200) lit++;
        }
        return lit;
      });
    };
    const sample = async () => {
      const levels = [];
      for (let i = 0; i < 4; i++) { levels.push(readLED()); await sleep(50); }
      return levels;
    };
    try {
      window.audioAnalyser = window.audioAnalyserL = analyser(0.1);
      window.audioAnalyserR = analyser(0.05);
      await sleep(500);
      const before = await sample();
      document.getElementById("tapeRecBtn").click();
      await sleep(300);
      if (!document.getElementById("tapeDeck").classList.contains("recording"))
        throw new Error("The meter regression must exercise an active recording");
      const during = await sample();
      return { before, during };
    } finally {
      document.getElementById("tapeStopBtn").click();
      for (let i = 0; i < 30 && document.getElementById("tapeDeck").classList.contains("recording"); i++) await sleep(25);
      document.getElementById("tapeDiscardBtn").click();
      [window.audioAnalyser, window.audioAnalyserL, window.audioAnalyserR] = originals;
    }
  });
  assert.notEqual(readings.before[0][0], readings.before[0][1], "Fixture must exercise independent stereo levels");
  assert.deepEqual(readings.during, readings.before, "Recording must keep the same readings for an unchanged stereo signal");
}
