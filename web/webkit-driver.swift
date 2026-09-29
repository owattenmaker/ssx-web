// WebKit QA driver (web/webkit-driver.mjs compiles and runs it): the macOS system WebKit (the engine and WebGPU of
// Safari) in a WKWebView, driven over stdin/stdout. safaridriver needs Safari's "Allow Remote Automation" and often
// times out connecting (docs/asset-formats.md "Texture archives"); this needs nothing but the Xcode command line tools.
//   webkit-driver URL WIDTH HEIGHT [--offscreen]
//   stdin, one JSON command per line: {"js": "<async function body>"} | {"goto": url} | {"shot": path, "rect": [x,y,w,h]} | {"quit": 1}
//   stdout, one JSON line per command: {"ok": true, "value": <JSON string of the result>} / {"ok": false, "error": ...};
//   events {"event": "loaded" | "crashed"}.
// --offscreen puts the (borderless) window outside every screen: fine for fetch / WebGPU work, while rAF-driven pages
// (the game) need the window on screen.
import AppKit
import WebKit

let args = CommandLine.arguments
let startURL = URL(string: args.count > 1 ? args[1] : "about:blank")!
let W = CGFloat(Double(args.count > 2 ? args[2] : "1280") ?? 1280), H = CGFloat(Double(args.count > 3 ? args[3] : "960") ?? 960)
let offscreen = args.contains("--offscreen")

func emit(_ obj: [String: Any]) {
  if let d = try? JSONSerialization.data(withJSONObject: obj, options: [.fragmentsAllowed]), let s = String(data: d, encoding: .utf8) { print(s); fflush(stdout) }
}

final class Driver: NSObject, WKNavigationDelegate {
  let web: WKWebView
  let window: NSWindow
  init(_ w: CGFloat, _ h: CGFloat) {
    let cfg = WKWebViewConfiguration()
    cfg.mediaTypesRequiringUserActionForPlayback = []
    // _setPageMuted: silences media elements but not Web Audio: a game page's AudioContext still played (Owen heard a
    // WebKit long-ride run). So, unless WEBKIT_DRIVER_AUDIO=1, every realtime AudioContext's destination is a gain-0 node in
    // front of the real one (timing, analysers and the audio engine's state run as normal; OfflineAudioContext untouched),
    // and media elements start muted.
    if ProcessInfo.processInfo.environment["WEBKIT_DRIVER_AUDIO"] != "1" {
      let silence = """
      (() => {
        const B = window.BaseAudioContext || window.AudioContext; if (!B) return;
        const d = Object.getOwnPropertyDescriptor(B.prototype, 'destination'); if (!d || !d.get) return;
        const muted = new WeakMap();
        Object.defineProperty(B.prototype, 'destination', { configurable: true, get() {
          const real = d.get.call(this);
          if (window.OfflineAudioContext && this instanceof window.OfflineAudioContext) return real;
          let g = muted.get(this);
          if (!g) { g = this.createGain(); g.gain.value = 0; g.connect(real);
            Object.defineProperty(g, 'maxChannelCount', { get: () => real.maxChannelCount }); muted.set(this, g); }
          return g;
        } });
        window.__webkitDriverSilent = true;
        const play = HTMLMediaElement.prototype.play;
        HTMLMediaElement.prototype.play = function () { this.muted = true; return play.apply(this, arguments); };
      })();
      """
      cfg.userContentController.addUserScript(WKUserScript(source: silence, injectionTime: .atDocumentStart, forMainFrameOnly: false))
    }
    web = WKWebView(frame: NSRect(x: 0, y: 0, width: w, height: h), configuration: cfg)
    if #available(macOS 13.3, *) { web.isInspectable = true }
    // Test runs must be silent: mute the whole page (WebKit's _setPageMuted:, _WKMediaAudioMuted = 1), not only pages
    // loaded with ?mute=1. WEBKIT_DRIVER_AUDIO=1 keeps the sound.
    let muteSel = Selector(("_setPageMuted:"))
    if ProcessInfo.processInfo.environment["WEBKIT_DRIVER_AUDIO"] != "1" && web.responds(to: muteSel) {
      typealias SetMuted = @convention(c) (AnyObject, Selector, UInt) -> Void
      unsafeBitCast(web.method(for: muteSel), to: SetMuted.self)(web, muteSel, 1)
    }
    if ProcessInfo.processInfo.environment["WEBKIT_DRIVER_DEBUG"] == "1" {
      FileHandle.standardError.write("webkit-driver: _setPageMuted: \(web.responds(to: muteSel)), _mediaMutedState = \(String(describing: web.value(forKey: "_mediaMutedState")))\n".data(using: .utf8)!)
    }
    let origin = offscreen ? NSPoint(x: -20000, y: -20000) : NSPoint(x: 40, y: 40)
    window = NSWindow(contentRect: NSRect(origin: origin, size: NSSize(width: w, height: h)), styleMask: offscreen ? [.borderless] : [.titled], backing: .buffered, defer: false)
    window.title = "WebKit QA (automation)"
    window.contentView = web
    super.init()
    web.navigationDelegate = self
    window.orderFrontRegardless()
    if offscreen { window.setFrameOrigin(origin) }
  }
  func run(_ cmd: [String: Any], done: @escaping () -> Void) {
    if let js = cmd["js"] as? String {
      web.callAsyncJavaScript("return JSON.stringify(await (async()=>{" + js + "})())", arguments: [:], in: nil, in: .page) { r in
        switch r { case .success(let v): emit(["ok": true, "value": v]); case .failure(let e): emit(["ok": false, "error": "\(e)"]) }
        done()
      }
    } else if let path = cmd["shot"] as? String {
      let conf = WKSnapshotConfiguration()
      if let rect = cmd["rect"] as? [Double], rect.count == 4 { conf.rect = NSRect(x: rect[0], y: rect[1], width: rect[2], height: rect[3]) }
      conf.afterScreenUpdates = true
      web.takeSnapshot(with: conf) { img, err in
        if let img = img, let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff), let png = rep.representation(using: .png, properties: [:]) {
          do { try png.write(to: URL(fileURLWithPath: path)); emit(["ok": true, "value": path, "w": rep.pixelsWide, "h": rep.pixelsHigh]) } catch { emit(["ok": false, "error": "\(error)"]) }
        } else { emit(["ok": false, "error": "snapshot failed: \(String(describing: err))"]) }
        done()
      }
    } else if let u = cmd["goto"] as? String, let url = URL(string: u) {
      web.load(URLRequest(url: url)); emit(["ok": true]); done()
    } else if cmd["quit"] != nil {
      emit(["ok": true]); exit(0)
    } else { emit(["ok": false, "error": "unknown command"]); done() }
  }
  func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) { emit(["event": "loaded"]) }
  func webViewWebContentProcessDidTerminate(_ webView: WKWebView) { emit(["event": "crashed"]) }
}

let app = NSApplication.shared
app.setActivationPolicy(.accessory)
let driver = Driver(W, H)
driver.web.load(URLRequest(url: startURL))
DispatchQueue.global().async {
  while let line = readLine() {
    guard let d = line.data(using: .utf8), let cmd = (try? JSONSerialization.jsonObject(with: d)) as? [String: Any] else { emit(["ok": false, "error": "bad command"]); continue }
    let sem = DispatchSemaphore(value: 0)
    DispatchQueue.main.async { driver.run(cmd) { sem.signal() } }
    sem.wait()
  }
  exit(0)
}
app.run()
