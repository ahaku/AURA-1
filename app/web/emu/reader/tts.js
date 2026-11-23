// SPDX-License-Identifier: MIT
// Built-in voice using browser Web Speech API (speechSynthesis)

export async function createTTS() {
  if (typeof window === "undefined" || !("speechSynthesis" in window)) {
    throw new Error("Speech synthesis not supported in this browser");
  }

  function unlock() {
    if (window.speechSynthesis.paused) {
      window.speechSynthesis.resume();
    }
  }

  function stop() {
    window.speechSynthesis.cancel();
  }

  function speak(text, { interrupt = true } = {}) {
    if (!text || !text.trim()) return Promise.resolve();
    if (interrupt) {
      window.speechSynthesis.cancel();
    }
    return new Promise((resolve) => {
      const u = new SpeechSynthesisUtterance(text);
      u.rate = 1.05;
      u.lang = "en-US";
      u.onend = () => resolve();
      u.onerror = () => resolve();
      window.speechSynthesis.speak(u);
    });
  }

  return { unlock, stop, speak };
}
