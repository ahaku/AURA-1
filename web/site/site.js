/* Quiet studio: native interactions, no automatic audio playback. */
(() => {
  const preview = document.querySelector("#instrument-preview");
  const listen = document.querySelector("#listen-button");
  const stop = document.querySelector("#preview-stop");
  const state = document.querySelector("#preview-state");
  let isPlaying = false;
  async function play(piece) {
    if (!preview || !listen) return;
    listen.disabled = true;
    try {
      await new Promise((resolve, reject) => {
        if (preview.contentWindow?.AuraEmbed) return resolve();
        const started = Date.now();
        const poll = setInterval(() => {
          if (preview.contentWindow?.AuraEmbed) {
            clearInterval(poll);
            resolve();
          } else if (Date.now() - started > 12000) {
            clearInterval(poll);
            reject(
              new Error(
                "The instrument is still loading. Open the full instrument to play.",
              ),
            );
          }
        }, 80);
      });
      await preview.contentWindow.AuraEmbed.play(piece);
      isPlaying = true;
      document.body.classList.add("is-listening");
      listen.setAttribute("aria-pressed", "true");
      listen.querySelector(".button-label").textContent = "Pause listening";
      if (state) state.textContent = "Sound is on";
    } catch (error) {
      if (state) state.textContent = error.message;
    } finally {
      listen.disabled = false;
    }
  }
  function pause() {
    preview.contentWindow.AuraEmbed?.pause();
    isPlaying = false;
    document.body.classList.remove("is-listening");
    listen.setAttribute("aria-pressed", "false");
    listen.querySelector(".button-label").textContent = "Start listening";
    if (state) state.textContent = "Ready to play";
  }
  listen?.addEventListener("click", () => (isPlaying ? pause() : play()));
  stop?.addEventListener("click", pause);
  document.querySelectorAll("[data-listen-piece]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      play(Number(button.dataset.listenPiece));
      preview?.scrollIntoView({
        block: "center",
        behavior: matchMedia("(prefers-reduced-motion: reduce)").matches
          ? "instant"
          : "smooth",
      });
    });
  });
  document.querySelectorAll("[data-listen-random]").forEach((button) => {
    button.addEventListener("click", async (event) => {
      event.preventDefault();
      listen.disabled = true;
      try {
        await new Promise((resolve, reject) => {
          if (preview.contentWindow?.AuraEmbed) return resolve();
          const started = Date.now();
          const poll = setInterval(() => {
            if (preview.contentWindow?.AuraEmbed) {
              clearInterval(poll);
              resolve();
            } else if (Date.now() - started > 12000) {
              clearInterval(poll);
              reject(new Error("The instrument is still loading."));
            }
          }, 80);
        });
        await preview.contentWindow.AuraEmbed.random();
        isPlaying = true;
        document.body.classList.add("is-listening");
        listen.setAttribute("aria-pressed", "true");
        listen.querySelector(".button-label").textContent = "Pause listening";
        if (state) state.textContent = "A new random world is playing";
      } catch (error) {
        if (state) state.textContent = error.message;
      } finally {
        listen.disabled = false;
      }
      preview?.scrollIntoView({
        block: "center",
        behavior: matchMedia("(prefers-reduced-motion: reduce)").matches
          ? "instant"
          : "smooth",
      });
    });
  });
  window.addEventListener("pagehide", () => {
    if (preview && listen) pause();
  });

  const manual = document.querySelector(".manual-content");
  const query = document.querySelector("#guide-search");
  const results = document.querySelector("#guide-results");
  const searchStatus = document.querySelector("#guide-search-status");
  if (manual && query && results) {
    const entries = [...manual.querySelectorAll("h2,h3,p,li,td")].filter(
      (node) =>
        node.textContent.trim().length > 8 && node.textContent.length < 1100,
    );
    query.addEventListener("input", () => {
      results.replaceChildren();
      const term = query.value.trim().toLowerCase();
      if (term.length < 2) {
        searchStatus.textContent = "";
        return;
      }
      const matches = entries
        .filter((node) => node.textContent.toLowerCase().includes(term))
        .slice(0, 7);
      matches.forEach((node, i) => {
        if (!node.id) node.id = `guide-match-${entries.indexOf(node)}`;
        const text = node.textContent.trim().replace(/\s+/g, " ");
        const at = text.toLowerCase().indexOf(term);
        const item = document.createElement("li"),
          link = document.createElement("a");
        link.href = "#" + node.id;
        link.textContent =
          (at > 30 ? "…" : "") +
          text.slice(Math.max(0, at - 30), at + 90) +
          (text.length > at + 90 ? "…" : "");
        link.addEventListener("click", () => {
          for (
            let ancestor = node.parentElement;
            ancestor;
            ancestor = ancestor.parentElement
          ) {
            if (ancestor.tagName === "DETAILS") ancestor.open = true;
          }
          node.setAttribute("tabindex", "-1");
          node.focus({ preventScroll: true });
        });
        item.append(link);
        results.append(item);
      });
      searchStatus.textContent = matches.length
        ? `${matches.length}${matches.length === 7 ? "+" : ""} results`
        : "No results. Try a control or parameter name.";
    });
    query.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        query.value = "";
        results.replaceChildren();
        searchStatus.textContent = "";
      }
    });
    const links = [...document.querySelectorAll(".manual-sidebar .contents a")];
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (!entry.isIntersecting) return;
          links.forEach((link) => {
            if (link.hash === "#" + entry.target.id)
              link.setAttribute("aria-current", "location");
            else link.removeAttribute("aria-current");
          });
        });
      },
      { rootMargin: "-5% 0px -70% 0px" },
    );
    manual.querySelectorAll("h2[id]").forEach((node) => observer.observe(node));
  }
  document.querySelectorAll(".image-zoom").forEach((button) =>
    button.addEventListener("click", () => {
      const original = button.querySelector("img");
      const dialog = document.createElement("dialog");
      dialog.className = "image-viewer";
      dialog.setAttribute("aria-label", original.alt || "Instrument controls");
      const close = document.createElement("button");
      close.type = "button";
      close.textContent = "Close image";
      const image = original.cloneNode();
      image.removeAttribute("width");
      image.removeAttribute("height");
      close.addEventListener("click", () => dialog.close());
      dialog.addEventListener("close", () => {
        dialog.remove();
        button.focus();
      });
      dialog.addEventListener("click", (event) => {
        if (event.target === dialog) dialog.close();
      });
      dialog.append(close, image);
      document.body.append(dialog);
      dialog.showModal();
    }),
  );
  const support = document.querySelector("#midi-support");
  if (support)
    support.textContent =
      "requestMIDIAccess" in navigator
        ? "Web MIDI is available"
        : "Use Chrome or Edge with Web MIDI";
})();
