import SwiftUI
import AVFoundation

@main
struct J2MELoaderApp: App {
    @StateObject private var gameManager = GameManager()
    @Environment(\.scenePhase) private var scenePhase
    @State private var backgroundTaskID: UIBackgroundTaskIdentifier = .invalid
    
    var body: some Scene {
        WindowGroup {
            LibraryView(gameManager: gameManager)
                .dynamicTypeSize(.medium)
                .environment(\.sizeCategory, .medium)
                .onOpenURL { url in
                    gameManager.importJar(from: url)
                }
                .onChange(of: scenePhase) { newPhase in
                    switch newPhase {
                    case .background:
                        handleAppEnteredBackground()
                    case .active:
                        handleAppBecameActive()
                    default:
                        break
                    }
                }
        }
    }
    
    private func handleAppEnteredBackground() {
        guard gameManager.isEmulating,
              let config = gameManager.currentGame?.config,
              config.backgroundKeepAlive else { return }
        
        // Ensure Audio Session is active for background processing
        AudioBridge.initializeAudio()
        
        // Begin persistent iOS background task
        backgroundTaskID = UIApplication.shared.beginBackgroundTask(withName: "J2HienLoader.ContinuousBackgroundEngine") {
            // Watchdog expiration handler - renew task to prevent iOS suspend
            self.endBackgroundTask()
            self.handleAppEnteredBackground()
        }
    }
    
    private func handleAppBecameActive() {
        endBackgroundTask()
    }
    
    private func endBackgroundTask() {
        if backgroundTaskID != .invalid {
            UIApplication.shared.endBackgroundTask(backgroundTaskID)
            backgroundTaskID = .invalid
        }
    }
}