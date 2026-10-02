import Flutter
import UIKit
import UniformTypeIdentifiers

@main
@objc class AppDelegate: FlutterAppDelegate, FlutterImplicitEngineDelegate {
  private var platformChannel: J2mePlatformChannel?

  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }

  func didInitializeImplicitFlutterEngine(_ engineBridge: FlutterImplicitEngineBridge) {
    GeneratedPluginRegistrant.register(with: engineBridge.pluginRegistry)
    if let registrar = engineBridge.pluginRegistry.registrar(forPlugin: "J2mePlatformChannel") {
      platformChannel = J2mePlatformChannel(messenger: registrar.messenger())
    }
  }
}

/// "j2me/platform" channel: app data directory and the .jar/.jad document picker.
final class J2mePlatformChannel: NSObject, UIDocumentPickerDelegate {
  private let channel: FlutterMethodChannel
  private var pendingPick: FlutterResult?
  private var exporting = false

  init(messenger: FlutterBinaryMessenger) {
    channel = FlutterMethodChannel(name: "j2me/platform", binaryMessenger: messenger)
    super.init()
    channel.setMethodCallHandler { [weak self] call, result in
      self?.handle(call, result: result)
    }
  }

  private func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "dataDir":
      // Documents is visible in the Files app (UIFileSharingEnabled), so saves and games can be backed up
      let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
      result(dir?.path)
    case "pickJar":
      pickJar(result: result)
    case "exportFile":
      guard let args = call.arguments as? [String: String],
            let path = args["path"], let name = args["name"] else {
        result(FlutterError(code: "export_failed", message: "Missing file", details: nil))
        return
      }
      exportFile(path: path, name: name, result: result)
    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private func pickJar(result: @escaping FlutterResult) {
    guard pendingPick == nil, let presenter = topViewController() else {
      result(nil)
      return
    }
    var types: [UTType] = [.data]
    for ext in ["jar", "jad"] {
      if let t = UTType(filenameExtension: ext) { types.insert(t, at: 0) }
    }
    let picker = UIDocumentPickerViewController(forOpeningContentTypes: types, asCopy: true)
    picker.delegate = self
    picker.allowsMultipleSelection = false
    pendingPick = result
    exporting = false
    presenter.present(picker, animated: true)
  }

  private func exportFile(path: String, name: String, result: @escaping FlutterResult) {
    guard pendingPick == nil, let presenter = topViewController() else {
      result(nil)
      return
    }
    let directory = FileManager.default.temporaryDirectory
      .appendingPathComponent("exported", isDirectory: true)
    let destination = directory.appendingPathComponent((name as NSString).lastPathComponent)
    do {
      try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
      if FileManager.default.fileExists(atPath: destination.path) {
        try FileManager.default.removeItem(at: destination)
      }
      try FileManager.default.copyItem(at: URL(fileURLWithPath: path), to: destination)
      let picker = UIDocumentPickerViewController(forExporting: [destination], asCopy: true)
      picker.delegate = self
      pendingPick = result
      exporting = true
      presenter.present(picker, animated: true)
    } catch {
      result(FlutterError(code: "export_failed", message: error.localizedDescription, details: nil))
    }
  }

  func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
    guard let result = pendingPick else { return }
    pendingPick = nil
    if exporting {
      exporting = false
      result(urls.first?.absoluteString)
      return
    }
    guard let src = urls.first else {
      result(nil)
      return
    }
    // The picker hands out a temporary copy; move it somewhere stable for the installer
    let dstDir = FileManager.default.temporaryDirectory.appendingPathComponent("picked", isDirectory: true)
    let dst = dstDir.appendingPathComponent(src.lastPathComponent)
    do {
      try FileManager.default.createDirectory(at: dstDir, withIntermediateDirectories: true)
      if FileManager.default.fileExists(atPath: dst.path) { try FileManager.default.removeItem(at: dst) }
      try FileManager.default.copyItem(at: src, to: dst)
      result(dst.path)
    } catch {
      result(FlutterError(code: "copy_failed", message: error.localizedDescription, details: nil))
    }
  }

  func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
    pendingPick?(nil)
    pendingPick = nil
    exporting = false
  }

  private func topViewController() -> UIViewController? {
    let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
    let window = scenes.flatMap { $0.windows }.first { $0.isKeyWindow } ?? scenes.first?.windows.first
    var top = window?.rootViewController
    while let presented = top?.presentedViewController { top = presented }
    return top
  }
}
