/* Additional touch surfaces drive the original emulator inputs and renderer. */
(() => {
  const embedded = document.documentElement.classList.contains("embedded");
  const panelControls = document.getElementById("controls");
  // The panel focuses div controls programmatically, which can preserve the
  // iframe's keyboard focus ring even on a pointer click.
  const keyboardFocus = () => { delete panelControls.dataset.input; };
  panelControls.addEventListener("pointerdown", () => {
    panelControls.dataset.input = "pointer";
  }, true);
  document.addEventListener("keydown", keyboardFocus, true);
  window.addEventListener("blur", keyboardFocus);
  const consoleEl = document.getElementById("mobile-console");
  if (consoleEl) {
    consoleEl.inert = true;
    const dial = (label, knobName) => {
      const box = document.createElement("div");
      box.className = "touch-dial";
      box.setAttribute("role", "group");
      box.setAttribute("aria-label", label);
      const title = document.createElement("label");
      title.textContent = label;
      const controls = document.createElement("div");
      controls.className = "dial-buttons";
      for (const [symbol, n] of [
        ["−", -1],
        ["+", 1],
      ]) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = symbol;
        button.setAttribute(
          "aria-label",
          `${n < 0 ? "Decrease" : "Increase"} ${label}`,
        );
        button.addEventListener("click", () => turn(knobs[knobName], n));
        controls.append(button);
      }
      box.append(title, controls);
      return box;
    };
    const selectors = document.createElement("div");
    selectors.className = "touch-selectors";
    selectors.append(
      dial("Piece", "PRESETS"),
      dial("Scale", "ALGORITHM"),
      dial("Page", "SELECT"),
    );
    const parameters = document.createElement("div");
    parameters.className = "touch-parameters";
    for (let i = 1; i <= 4; i++)
      parameters.append(dial(`Knob ${i}`, `KNOB${i}`));
    const bindPress = (element, kind, value) => {
      let pointerAt = -10000;
      const up = (event) => {
        element.classList.remove("is-held");
        letGo("touch-" + event.pointerId);
      };
      element.addEventListener("pointerdown", (event) => {
        if (!node) return;
        event.preventDefault();
        pointerAt = performance.now();
        element.setPointerCapture(event.pointerId);
        element.classList.add("is-held");
        holdDown("touch-" + event.pointerId, kind, value);
      });
      for (const event of ["pointerup", "pointercancel", "lostpointercapture"])
        element.addEventListener(event, up);
      element.addEventListener("keydown", (event) => {
        if (
          (event.key === " " || event.key === "Enter") &&
          !event.repeat &&
          node
        ) {
          event.preventDefault();
          holdDown("touch-keyboard-" + kind + value, kind, value);
          element.classList.add("is-held");
        }
      });
      element.addEventListener("keyup", (event) => {
        if (event.key === " " || event.key === "Enter") {
          event.preventDefault();
          letGo("touch-keyboard-" + kind + value);
          element.classList.remove("is-held");
        }
      });
      element.addEventListener("click", (event) => {
        if (
          event.detail === 0 &&
          node &&
          performance.now() - pointerAt > 1000
        ) {
          const id = "touch-accessible-" + kind + value;
          holdDown(id, kind, value);
          setTimeout(() => letGo(id), 80);
        }
      });
      element.addEventListener("blur", () => {
        letGo("touch-keyboard-" + kind + value);
        element.classList.remove("is-held");
      });
    };
    const press = (label, key) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "touch-press";
      button.textContent = label;
      button.dataset.hardwareButton = key;
      button.setAttribute("aria-label", buttons[key].name);
      bindPress(button, "btn", BTN[key]);
      return button;
    };
    const transport = document.createElement("div");
    transport.className = "touch-transport";
    for (const [label, key] of [
      ["Play / rest", "PLAY"],
      ["Freeze", "REC"],
      ["Solo", "SEL"],
      ["Next chord", "EDIT"],
    ])
      transport.append(press(label, key));
    const pages = document.createElement("div");
    pages.className = "touch-pages";
    for (const [label, key] of [
      ["Garden", "HOME"],
      ["Pad", "ENV"],
      ["Bells", "ARP"],
      ["Bass", "LFO"],
      ["Pulse", "SEQ"],
      ["Space", "FX"],
      ["Setup", "GLO"],
      ["Save", "SAVE"],
      ["Octave −", "OCT-"],
      ["Octave +", "OCT+"],
    ])
      pages.append(press(label, key));
    const notes = document.createElement("section");
    notes.className = "touch-notes";
    const h = document.createElement("h2");
    h.textContent = "Your notes";
    const noteHelp = document.createElement("p");
    noteHelp.textContent =
      "Tap notes to shape the scale. In Solo mode, hold them to play. Hold Garden for the cosmic screen.";
    const grid = document.createElement("div");
    grid.className = "touch-key-grid";
    const names = [
      "C",
      "C♯",
      "D",
      "D♯",
      "E",
      "F",
      "F♯",
      "G",
      "G♯",
      "A",
      "A♯",
      "B",
    ];
    for (let i = 0; i < 27; i++) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = names[(i + 5) % 12];
      if (button.textContent.includes("♯")) button.className = "sharp";
      button.dataset.hardwareKey = i;
      button.setAttribute("aria-label", keys[i].name);
      bindPress(button, "key", i);
      grid.append(button);
    }
    notes.append(h, noteHelp, grid);
    const volume = document.createElement("label");
    volume.className = "touch-volume";
    volume.textContent = "Volume";
    const range = document.createElement("input");
    range.type = "range";
    range.min = "0";
    range.max = "64";
    range.value = String(Math.round(master / 64));
    range.setAttribute("aria-label", "Master volume");
    range.addEventListener("input", () =>
      turn(knobs.MASTER, Number(range.value) - Math.round(master / 64)),
    );
    volume.append(range);
    const readout = document.createElement("div");
    readout.className = "touch-readout";
    readout.setAttribute("aria-hidden", "true");
    const monitor = document.createElement("canvas");
    monitor.id = "touch-mini-screen";
    monitor.width = monitor.height = 240;
    const value = document.createElement("p");
    value.textContent =
      "Your current page and control values stay here while you play.";
    readout.append(monitor, value);
    const readoutObserver = new MutationObserver(() => {
      const text = document.getElementById("said").textContent.trim();
      if (text) {
        value.textContent = text;
        value.title = text;
      }
    });
    readoutObserver.observe(document.getElementById("said"), {
      childList: true,
      characterData: true,
      subtree: true,
    });
    consoleEl.append(
      readout,
      selectors,
      parameters,
      transport,
      pages,
      notes,
      volume,
    );
    const lights = new MutationObserver(() => {
      consoleEl
        .querySelectorAll("[data-hardware-button]")
        .forEach((element) =>
          element.classList.toggle(
            "is-lit",
            buttons[element.dataset.hardwareButton].g.classList.contains("lit"),
          ),
        );
      consoleEl
        .querySelectorAll("[data-hardware-key]")
        .forEach((element) =>
          element.classList.toggle(
            "is-lit",
            keys[Number(element.dataset.hardwareKey)].g.classList.contains(
              "lit",
            ),
          ),
        );
    });
    lights.observe(document.getElementById("panel"), {
      subtree: true,
      attributes: true,
      attributeFilter: ["class"],
    });
  }
  document
    .getElementById("mobile-power")
    ?.addEventListener("click", async (event) => {
      event.currentTarget.disabled = true;
      await switchOn();
      if (!node) document.getElementById("mobile-power").disabled = false;
    });
  const toast = document.getElementById("pwa-toast");
  if (toast) {
    document.querySelector(".instrument-main").before(toast);
    const dismiss = document.createElement("button");
    dismiss.type = "button";
    dismiss.className = "toast-dismiss";
    dismiss.textContent = "Later";
    dismiss.addEventListener("click", () => {
      toast.style.display = "none";
    });
    toast.append(dismiss);
  }
  let startPromise = null;
  window.AuraEmbed = {
    async play(nextPiece) {
      if (!node && !startPromise) startPromise = switchOn();
      if (startPromise) await startPromise;
      if (!node) {
        startPromise = null;
        throw new Error(
          "The instrument could not start. Open the full instrument to try again.",
        );
      }
      if (window.audioCtx) await window.audioCtx.resume();
      readerOn();
      document.getElementById("mute").setAttribute("aria-pressed", "false");
      if (gain) gain.gain.value = 1;
      if (
        Number.isInteger(nextPiece) &&
        nextPiece >= 0 &&
        nextPiece < 16
      ) {
        send({ type: "piece", index: nextPiece });
      }
    },
    async random() {
      await this.play();
      send({ type: "random" });
    },
    pause() {
      if (gain) gain.gain.value = 0;
      document.getElementById("mute").setAttribute("aria-pressed", "true");
      if (embedded) {
        readerOff();
        window.audioCtx?.suspend();
      }
    },
  };
  if (embedded) {
    document.getElementById("reset").disabled = true;
    document.getElementById("shortcuts").checked = false;
  }
})();
