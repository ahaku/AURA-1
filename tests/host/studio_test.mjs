// Browser regression checks for the built studio site. Does not write to hardware.
// Run from a directory with playwright installed:
//   node /absolute/path/tests/host/studio_test.mjs http://127.0.0.1:8080/
import assert from "node:assert/strict";
import { createRequire } from "node:module";
import path from "node:path";
import { checkRecordingMeter } from "./recording_meter_check.mjs";

const require = createRequire(
  path.join(process.cwd(), "studio-test-loader.js"),
);
const { chromium } = require("playwright");
const base = (process.argv[2] || "http://127.0.0.1:8080/").replace(/\/?$/, "/");
let browser;
for (const channel of [undefined, "chrome", "msedge"]) {
  try {
    browser = await chromium.launch({
      channel,
      headless: true,
      args: ["--enable-unsafe-swiftshader"],
    });
    break;
  } catch {}
}
assert(browser, "A Chromium browser is required for the UI checks");
const errors = [];
const pageOf = async (context) => {
  const page = await context.newPage();
  page.on("pageerror", (error) => errors.push(error.message));
  return page;
};

try {
  const desktop = await browser.newContext({
    viewport: { width: 1440, height: 1000 },
  });
  const page = await pageOf(desktop);
  await page.goto(base, { waitUntil: "networkidle" });
  await page.locator("#listen-button").click();
  await page.waitForFunction(() =>
    document.body.classList.contains("is-listening"),
  );
  const preview = page
    .frames()
    .find((frame) => frame.url().includes("embed=1"));
  assert.equal(await preview.evaluate(() => window.audioCtx.state), "running");
  assert.equal(await preview.locator("#bg-canvas").count(), 0, "The landing preview must not create an animated background canvas");
  const padControl = preview.getByRole("button", { name: /^ENV/ });
  await padControl.click({ delay: 80, timeout: 2000 });
  assert.equal(await padControl.evaluate((element) => getComputedStyle(element).outlineStyle), "none", "Pointer clicks must not show a keyboard focus ring");
  await page.keyboard.press("Tab");
  assert.equal(await preview.evaluate(() => getComputedStyle(document.activeElement).outlineStyle), "solid", "Tab navigation must retain its focus ring");
  await padControl.click({ delay: 80, timeout: 2000 });
  assert.equal(await padControl.evaluate((element) => getComputedStyle(element).outlineStyle), "none", "Returning to pointer input must remove the keyboard focus ring");
  await preview.waitForFunction(() => buttons.ENV.g.classList.contains("lit"), null, { timeout: 2000 });
  const knob = preview.getByRole("button", { name: "KNOB 1", exact: true });
  const angle = await preview.evaluate(() => knobs.KNOB1.angle);
  const knobBox = await knob.boundingBox();
  await page.mouse.move(knobBox.x + knobBox.width / 2, knobBox.y + knobBox.height / 2);
  await page.mouse.wheel(0, -100);
  await preview.waitForFunction((old) => knobs.KNOB1.angle !== old, angle, { timeout: 2000 });
  await preview.waitForFunction(() => document.getElementById("said").textContent.includes("Level"), null, { timeout: 2000 });
  const noteWasLit = await preview.evaluate(() => keys[0].g.classList.contains("lit"));
  await preview.getByRole("button", { name: /^White key 1(,|$)/ }).click({ delay: 80, timeout: 2000 });
  await preview.waitForFunction((old) => keys[0].g.classList.contains("lit") !== old, noteWasLit, { timeout: 2000 });
  console.log("ok: landing instrument buttons, knob and note accept pointer input");
  await page.getByRole("link", { name: /Bamboo/ }).click();
  await preview.waitForFunction(() => window.emuLast?.piece === 13, null, { timeout: 5000 });
  console.log("ok: Bamboo card selects the Bamboo preset");
  await preview.evaluate(() => {
    window.__pieceTrace = [];
    window.__pieceTraceObserver = new MutationObserver(() => {
      const said = document.getElementById("said").textContent;
      if (said.startsWith("piece ") && window.__pieceTrace.at(-1) !== said)
        window.__pieceTrace.push(said);
    });
    window.__pieceTraceObserver.observe(document.getElementById("said"), {
      childList: true,
      subtree: true,
      characterData: true,
    });
  });
  await page.locator('[data-listen-piece="0"]').click();
  await preview.waitForFunction(() => window.emuLast?.piece === 0, null, { timeout: 5000 });
  const pieceTrace = await preview.evaluate(() => {
    window.__pieceTraceObserver.disconnect();
    return window.__pieceTrace;
  });
  assert.equal(pieceTrace.length, 1, `Preset selection must be direct, got: ${pieceTrace.join(" -> ")}`);
  assert(pieceTrace[0].includes("Garden"));
  console.log("ok: Bamboo switches directly to Garden without intermediate presets");
  const randomCard = page.locator("[data-listen-random]");
  assert.equal(
    await randomCard.evaluate((card) =>
      card.previousElementSibling?.textContent.includes("Bamboo"),
    ),
    true,
    "The random-world CTA must follow Bamboo",
  );
  const beforeRandom = await preview.locator("#said").textContent();
  await randomCard.click();
  await page.waitForFunction(
    () => document.getElementById("preview-state").textContent.includes("random world"),
  );
  await preview.waitForFunction(
    (before) => document.getElementById("said").textContent !== before,
    beforeRandom,
    { timeout: 5000 },
  );
  console.log("ok: random-world CTA follows Bamboo and generates a new world");
  await page.locator('[data-listen-piece="13"]').click();
  await preview.waitForFunction(
    () => document.getElementById("said").textContent.includes("Bamboo"),
    null,
    { timeout: 5000 },
  );
  console.log("ok: selecting Bamboo after random world restores and plays Bamboo");
  await page.locator("#listen-button").click();
  await page.waitForFunction(
    () => !document.body.classList.contains("is-listening"),
  );
  await preview.waitForFunction(() => window.audioCtx.state === "suspended");
  await page.locator("#listen-button").click();
  await preview.waitForFunction(() => window.audioCtx.state === "running");
  console.log("ok: live landing preview starts, pauses and resumes");

  await page.goto(new URL("manual/", base).href, { waitUntil: "networkidle" });
  await page.goBack({ waitUntil: "networkidle" });
  await page.waitForFunction(() => !document.body.classList.contains("is-listening"));
  await page.locator("#listen-button").click();
  await page.waitForFunction(() => document.body.classList.contains("is-listening"));
  console.log("ok: browser back restores an honest paused preview and can resume");
  await page.goto(new URL("manual/", base).href, { waitUntil: "networkidle" });
  await page.locator("#guide-search").fill("rain");
  const searchResults = page.locator("#guide-results a");
  const resultCount = await searchResults.count();
  assert(resultCount > 0);
  const resultTarget = await searchResults.first().getAttribute("href");
  await searchResults.first().click();
  assert.equal(await searchResults.count(), resultCount, "Selecting a search result must retain all matches");
  assert.equal(await page.locator("#guide-search").inputValue(), "rain");
  assert.equal(new URL(page.url()).hash, resultTarget);
  await page.locator(".manual-overview summary").click();
  await page.locator(".image-zoom").click();
  assert(await page.locator(".image-viewer").isVisible());
  await page.keyboard.press("Escape");
  assert.equal(await page.locator(".image-viewer").count(), 0);
  console.log("ok: guide search and enlarged controls diagram");

  const missing = await page.goto(
    new URL("a-page-that-does-not-exist", base).href,
  );
  assert.equal(missing.status(), 404);
  assert(
    (await page.getByRole("link", { name: /Open the instrument/ }).count()) > 0,
  );
  await page.goto(new URL("install/", base).href, { waitUntil: "networkidle" });
  assert.equal(await page.locator("#go").count(), 1);
  assert.equal(await page.locator("#confirmgo").count(), 1);
  // Deliberately do not click Install, confirm a dialog or request a MIDI port.
  console.log("ok: branded 404 and installer controls present");
  await desktop.close();

  const phone = await browser.newContext({
    viewport: { width: 390, height: 844 },
    isMobile: true,
    hasTouch: true,
  });
  const mobile = await pageOf(phone);
  await mobile.goto(new URL("emu/", base).href, { waitUntil: "networkidle" });
  assert.equal(
    await mobile.evaluate(
      () => document.documentElement.scrollWidth > innerWidth,
    ),
    false,
  );
  await mobile.locator("#mobile-power").click();
  await mobile.waitForFunction(() =>
    document.body.classList.contains("instrument-on"),
  );
  assert.equal(
    await mobile.locator("#mobile-console").evaluate((node) => node.inert),
    false,
  );
  await mobile.locator('[data-hardware-button="ENV"]').click();
  await mobile.waitForFunction(() =>
    document
      .querySelector('[data-hardware-button="ENV"]')
      .classList.contains("is-lit"),
  );
  await mobile
    .getByRole("button", { name: "Increase Knob 1", exact: true })
    .click();
  await mobile.waitForFunction(() =>
    document.querySelector(".touch-readout p").textContent.includes("Level"),
  );
  const key = mobile.locator('[data-hardware-key="0"]');
  const before = await key.evaluate((node) =>
    node.classList.contains("is-lit"),
  );
  await key.click();
  await mobile.waitForFunction(
    (old) =>
      document
        .querySelector('[data-hardware-key="0"]')
        .classList.contains("is-lit") !== old,
    before,
  );
  console.log("ok: touch page, parameter readout and scale key");

  await mobile.locator("#tapeBtn").click();
  const bounds = await mobile.locator("#tapeDeck").evaluate((node) => {
    const box = node.getBoundingClientRect();
    return { left: box.left, right: box.right, width: innerWidth };
  });
  assert(bounds.left >= 0 && bounds.right <= bounds.width);
  await checkRecordingMeter(mobile);
  await mobile.locator("#tapeRecBtn").click();
  await mobile.waitForTimeout(1000);
  await mobile.locator("#tapeStopBtn").click();
  await mobile.waitForFunction(
    () => !document.getElementById("tapeDownloadBtn").disabled,
  );
  const recordedBytes = await mobile.evaluate(
    async () => (await (await fetch(window.__currentBlobUrl)).blob()).size,
  );
  assert(recordedBytes > 44100, "WAV must contain the recorded audio frames");
  await mobile.locator("#tapeDiscardBtn").click();
  await mobile.locator("#tapeClose").click();
  await mobile.locator("#zenBtn").click();
  assert(
    await mobile.evaluate(() =>
      document.documentElement.classList.contains("zen"),
    ),
  );
  await mobile.locator("#zenExit").click();
  console.log("ok: recording produces a WAV; Zen opens and closes");

  await mobile.evaluate(() => navigator.serviceWorker.ready);
  await mobile.waitForFunction(() =>
    Boolean(navigator.serviceWorker.controller),
  );
  await phone.setOffline(true);
  await mobile.reload({ waitUntil: "networkidle" });
  await mobile.locator("#mobile-power").click();
  await mobile.waitForFunction(() =>
    document.body.classList.contains("instrument-on"),
  );
  await mobile.goto(new URL("emu/manual.html", base).href, {
    waitUntil: "networkidle",
  });
  const offlineImages = await mobile.evaluate(async () =>
    Promise.all(
      [...document.images].map(async (image) => (await fetch(image.src)).ok),
    ),
  );
  assert(
    offlineImages.length > 0 && offlineImages.every(Boolean),
    "Guide illustrations must be cached offline",
  );
  console.log("ok: instrument and illustrated guide work offline");
  await phone.close();

  const reduced = await browser.newContext({
    viewport: { width: 390, height: 844 },
    reducedMotion: "reduce",
  });
  const calm = await pageOf(reduced);
  await calm.goto(new URL("emu/", base).href, { waitUntil: "networkidle" });
  assert.equal(await calm.locator("#bg-canvas").count(), 0);
  assert(await calm.locator("#mobile-power").isVisible());
  await reduced.close();
  const noGraphics = await browser.newContext({
    viewport: { width: 390, height: 844 },
  });
  const fallback = await pageOf(noGraphics);
  await fallback.addInitScript(() => {
    window.__webglAttempts = 0;
    const original = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...args) {
      if (/webgl/.test(type)) {
        window.__webglAttempts++;
        return null;
      }
      return original.call(this, type, ...args);
    };
  });
  await fallback.goto(new URL("emu/", base).href, { waitUntil: "networkidle" });
  await fallback.locator("#mobile-power").click();
  await fallback.waitForFunction(() =>
    document.body.classList.contains("instrument-on"),
  );
  assert.equal(await fallback.evaluate(() => window.audioCtx.state), "running");
  assert.equal(await fallback.evaluate(() => window.__webglAttempts), 0, "The instrument must not allocate a background WebGL renderer");
  assert.equal(await fallback.locator("#bg-canvas, #bganim").count(), 0);
  await noGraphics.close();
  assert.deepEqual(errors, [], "No uncaught page exceptions");
  console.log(
    "ok: reduced motion and no-GPU fallback remain playable; no page exceptions",
  );
  await desktop.close();

  const shared = await browser.newContext({
    viewport: { width: 1280, height: 900 },
  });
  const sharedPage = await pageOf(shared);
  await sharedPage.goto(base, { waitUntil: "networkidle" });
  await sharedPage.locator('[data-listen-piece="13"]').click();
  let sharedPreview = sharedPage.frames().find((frame) => frame.url().includes("embed=1"));
  await sharedPreview.waitForFunction(
    () => window.emuLast?.piece === 13,
  );
  await sharedPage.waitForFunction(() =>
    Object.keys(JSON.parse(localStorage.getItem("aura-1-emu-store") || "{}")).length > 0,
  );
  await sharedPage.goto(new URL("emu/", base).href, { waitUntil: "networkidle" });
  await sharedPage.locator("#go").click();
  await sharedPage.waitForFunction(() =>
    window.emuLast?.piece === 13,
  );
  await sharedPage.evaluate(() => window.AuraEmbed.play(0));
  await sharedPage.waitForFunction(() =>
    window.emuLast?.piece === 0,
  );
  await sharedPage.goto(base, { waitUntil: "networkidle" });
  await sharedPage.locator("#listen-button").click();
  sharedPreview = sharedPage.frames().find((frame) => frame.url().includes("embed=1"));
  await sharedPreview.waitForFunction(() =>
    window.emuLast?.piece === 0,
  );
  await shared.close();
  console.log("ok: landing player and full emulator share synth state both ways");
} finally {
  await browser.close();
}
