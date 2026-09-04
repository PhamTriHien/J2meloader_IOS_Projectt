import SwiftUI

@main
struct J2MELoaderApp: App {
    @StateObject private var gameManager = GameManager()
    
    var body: some Scene {
        WindowGroup {
            LibraryView(gameManager: gameManager)
                .onOpenURL { url in
                    gameManager.importJar(from: url)
                }
        }
    }
}