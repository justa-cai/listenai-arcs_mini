/**
 * Improve embedded <video> UX in docs:
 * - Show poster before data is ready (handled by HTML poster attribute)
 * - Reduce black flash on loop by seeking slightly before end
 *
 * Only affects videos explicitly marked with data-seamless-loop="1".
 */
(function () {
  "use strict";

  // Note:
  // Many browsers fire `timeupdate` at a low frequency (e.g. ~4Hz), which can
  // easily miss a small "near end" window and still hit the real end (black
  // flash / decoder reset). Here we use a higher-frequency monitor:
  // - requestVideoFrameCallback (best, per decoded frame) when available
  // - setInterval fallback otherwise

  function shouldHandle(video) {
    return (
      video &&
      video.tagName === "VIDEO" &&
      video.dataset &&
      video.dataset.seamlessLoop === "1"
    );
  }

  function clamp(n, min, max) {
    if (!isFinite(n)) return min;
    if (n < min) return min;
    if (n > max) return max;
    return n;
  }

  function safePlay(video) {
    try {
      var p = video.play();
      if (p && typeof p.catch === "function") p.catch(function () {});
    } catch (e) {}
  }

  function installSeamlessLoop(video) {
    // IMPORTANT:
    // Some browsers still show a black flash even if we try to "pre-jump" while
    // native `loop` is enabled (decoder reset happens at the internal loop boundary).
    // So we explicitly disable native looping and fully handle the loop in JS.
    try {
      video.loop = false;
    } catch (e) {}

    // Some browsers show a black frame when reaching the end and restarting.
    // Avoid ever reaching duration exactly by jumping back slightly.
    // NOTE: threshold must be >= typical `timeupdate` interval to be reliable.
    // We still keep a larger default for safety, and also run high-frequency checks.
    var threshold = 0.9; // seconds (default larger to cover slow decoders)
    var targetTime = 0.05; // skip potential black first frame (can be overridden)
    if (video.dataset && video.dataset.loopStart) {
      var parsed = parseFloat(video.dataset.loopStart);
      if (isFinite(parsed) && parsed >= 0) targetTime = parsed;
    }
    if (video.dataset && video.dataset.loopThreshold) {
      var parsedTh = parseFloat(video.dataset.loopThreshold);
      if (isFinite(parsedTh) && parsedTh > 0) threshold = parsedTh;
    }
    // keep > 0 to avoid some keyframe/decoder edge cases
    if (targetTime === 0) targetTime = 0.001;
    threshold = clamp(threshold, 0.2, 2.0);

    var jumping = false;
    var lastJumpMs = 0;

    function shouldJumpNearEnd() {
      if (video.paused || video.seeking || jumping) return false;
      var d = video.duration;
      if (!isFinite(d) || d <= 0) return;
      // Ensure we have a current frame; otherwise seeking may produce a blank frame.
      if (video.readyState < 2) return false;
      return video.currentTime >= d - threshold;
    }

    function jumpToLoopStart() {
      if (jumping) return;
      var now = typeof performance !== "undefined" && performance.now ? performance.now() : Date.now();
      if (now - lastJumpMs < 150) return; // debounce
      lastJumpMs = now;
      jumping = true;

      var wasPaused = video.paused;
      try {
        video.currentTime = targetTime;
      } catch (e) {}
      if (!wasPaused) {
        // Some browsers pause briefly after a programmatic seek; ensure it keeps playing.
        // Also helps "carousel/tab switch" scenarios where the element is re-shown.
        var done = false;
        var onSeeked = function () {
          if (done) return;
          done = true;
          video.removeEventListener("seeked", onSeeked);
          jumping = false;
          safePlay(video);
        };
        video.addEventListener("seeked", onSeeked);
        // Fallback if seeked never fires (rare).
        setTimeout(function () {
          if (done) return;
          done = true;
          video.removeEventListener("seeked", onSeeked);
          jumping = false;
          safePlay(video);
        }, 400);
      } else {
        jumping = false;
      }
    }

    function monitor() {
      if (shouldJumpNearEnd()) jumpToLoopStart();
    }

    // Backup monitors: some browsers throttle frame callbacks while hidden.
    video.addEventListener("timeupdate", monitor, { passive: true });
    // If user scrubs to the end while paused, do not force-jump.
    video.addEventListener("ended", function () {
      if (video.paused) return;
      jumpToLoopStart();
    });

    // Avoid the "black first frame" for videos whose very first frame decodes to black.
    // Only do this for our explicitly configured loop videos.
    video.addEventListener("play", function () {
      if (video.currentTime <= 0.001 && targetTime > 0.001) {
        try {
          video.currentTime = targetTime;
        } catch (e) {}
      }
    });

    // High-frequency monitor: per decoded frame when supported.
    if (typeof video.requestVideoFrameCallback === "function") {
      var rvfc = function () {
        monitor();
        // Keep scheduling while element exists.
        if (document.contains(video)) video.requestVideoFrameCallback(rvfc);
      };
      video.requestVideoFrameCallback(rvfc);
    } else {
      // Fallback: 20Hz is enough to reliably catch the near-end window.
      var intervalId = setInterval(function () {
        // Stop once the element is removed.
        if (!document.contains(video)) {
          clearInterval(intervalId);
          return;
        }
        monitor();
      }, 50);
    }
  }

  function installVisibilityHandling(videos) {
    // Pause videos when the tab is hidden, and resume those that were playing.
    // This reduces glitches/black frames when users "carousel" between tabs/pages.
    var resumeSet = new WeakMap();
    document.addEventListener("visibilitychange", function () {
      var hidden = !!document.hidden;
      for (var i = 0; i < videos.length; i++) {
        var v = videos[i];
        if (!v || v.tagName !== "VIDEO") continue;
        if (hidden) {
          if (!v.paused) {
            resumeSet.set(v, true);
            try {
              v.pause();
            } catch (e) {}
          }
        } else {
          if (resumeSet.get(v)) {
            resumeSet.delete(v);
            safePlay(v);
          }
        }
      }
    });
  }

  function init() {
    var videos = document.querySelectorAll('video[data-seamless-loop="1"]');
    var handled = [];
    for (var i = 0; i < videos.length; i++) {
      var v = videos[i];
      if (!shouldHandle(v)) continue;
      installSeamlessLoop(v);
      handled.push(v);
    }
    if (handled.length) installVisibilityHandling(handled);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();

