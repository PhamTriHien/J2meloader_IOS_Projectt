import SwiftUI

public struct GameScreenView: View {
    public var game: GameItem
    @ObservedObject var gameManager: GameManager
    
    @State private var isPaused: Bool = false
    @State private var showingSettings: Bool = false
    @State private var currentConfig: EmulatorConfig
    @Environment(\.presentationMode) var presentationMode
    
    public init(game: GameItem, gameManager: GameManager) {
        self.game = game
        self.gameManager = gameManager
        _currentConfig = State(initialValue: game.config)
    }
    
    public var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            
            VStack(spacing: 0) {
                // Top Control Bar
                HStack {
                    Button(action: {
                        gameManager.stopEmulation()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        Image(systemName: "chevron.backward.circle.fill")
                            .font(.system(size: 26))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    Spacer()
                    
                    Text(game.title)
                        .font(.headline)
                        .foregroundColor(.white)
                        .lineLimit(1)
                    
                    Spacer()
                    
                    Button(action: {
                        isPaused.toggle()
                        J2MEBridge.setPaused(isPaused)
                    }) {
                        Image(systemName: isPaused ? "play.circle.fill" : "pause.circle.fill")
                            .font(.system(size: 26))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    Button(action: { showingSettings = true }) {
                        Image(systemName: "gearshape.fill")
                            .font(.system(size: 22))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    .padding(.leading, 8)
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 8)
                .background(Color(.darkGray).opacity(0.4))
                
                // Emulation Canvas (Metal LCDUI Screen)
                ZStack {
                    Color.black
                    
                    MetalView(config: currentConfig) { x, y, action in
                        J2MEBridge.sendTouchEvent(x, y: y, action: action)
                    }
                    .aspectRatio(CGFloat(currentConfig.effectiveWidth) / CGFloat(currentConfig.effectiveHeight), contentMode: .fit)
                    .clipped()
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                
                // On-screen Virtual Keypad
                if currentConfig.keypadLayout != .hidden {
                    VirtualKeypadView(config: currentConfig) { keyCode, isDown in
                        J2MEBridge.sendKeyEvent(keyCode, isDown: isDown)
                    }
                    .padding(.bottom, 12)
                }
            }
        }
        .onAppear {
            let jarURL = gameManager.gamesDirectory.appendingPathComponent(game.jarFileName)
            J2MEBridge.startEmulator(
                jarURL.path,
                mainClass: game.mainClass,
                width: Int32(currentConfig.effectiveWidth),
                height: Int32(currentConfig.effectiveHeight),
                soundEnabled: currentConfig.soundEnabled
            )
        }
        .onDisappear {
            J2MEBridge.stopEmulator()
        }
        .sheet(isPresented: $showingSettings) {
            SettingsView(game: game) { updated in
                self.currentConfig = updated.config
                gameManager.updateGame(updated)
            }
        }
        .statusBar(hidden: true)
    }
}