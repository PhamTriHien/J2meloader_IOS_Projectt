import SwiftUI

public struct GameScreenView: View {
    public var game: GameItem
    @ObservedObject var gameManager: GameManager
    
    @State private var isPaused: Bool = false
    @State private var speedMultiplier: Int = 1 // 1x, 2x, 4x
    @State private var showingSettings: Bool = false
    @State private var currentConfig: EmulatorConfig
    @State private var showingScreenshotToast: Bool = false
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
                // Top Retro Action Bar
                HStack(spacing: 12) {
                    Button(action: {
                        gameManager.stopEmulation()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        Image(systemName: "chevron.backward.circle.fill")
                            .font(.system(size: 24))
                            .foregroundColor(.white.opacity(0.9))
                    }
                    
                    VStack(alignment: .leading, spacing: 1) {
                        Text(game.title)
                            .font(.system(size: 15, weight: .bold))
                            .foregroundColor(.white)
                            .lineLimit(1)
                        
                        Text("\(currentConfig.effectiveWidth)x\(currentConfig.effectiveHeight) • \(currentConfig.targetFps * speedMultiplier) FPS")
                            .font(.system(size: 10, weight: .medium))
                            .foregroundColor(.white.opacity(0.6))
                    }
                    
                    Spacer()
                    
                    // Fast-Forward Speed Toggle (1x, 2x, 4x)
                    Button(action: {
                        if speedMultiplier == 1 { speedMultiplier = 2 }
                        else if speedMultiplier == 2 { speedMultiplier = 4 }
                        else { speedMultiplier = 1 }
                    }) {
                        Text("\(speedMultiplier)x")
                            .font(.system(size: 12, weight: .bold))
                            .padding(.horizontal, 6)
                            .padding(.vertical, 3)
                            .background(speedMultiplier > 1 ? Color.accentColor : Color.white.opacity(0.15))
                            .foregroundColor(.white)
                            .cornerRadius(6)
                    }
                    
                    // Restart MIDlet Button
                    Button(action: { restartEmulation() }) {
                        Image(systemName: "arrow.counterclockwise.circle.fill")
                            .font(.system(size: 22))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Pause / Resume Button
                    Button(action: {
                        isPaused.toggle()
                        J2MEBridge.setPaused(isPaused)
                    }) {
                        Image(systemName: isPaused ? "play.circle.fill" : "pause.circle.fill")
                            .font(.system(size: 24))
                            .foregroundColor(isPaused ? .yellow : .white.opacity(0.85))
                    }
                    
                    // Quick Settings
                    Button(action: { showingSettings = true }) {
                        Image(systemName: "gearshape.fill")
                            .font(.system(size: 20))
                            .foregroundColor(.white.opacity(0.85))
                    }
                }
                .padding(.horizontal, 14)
                .padding(.vertical, 8)
                .background(Color(.darkGray).opacity(0.45))
                
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
                    .padding(.bottom, 8)
                }
            }
        }
        .onAppear {
            startEmulation()
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
    
    private func startEmulation() {
        let jarURL = gameManager.gamesDirectory.appendingPathComponent(game.jarFileName)
        J2MEBridge.startEmulator(
            jarURL.path,
            mainClass: game.mainClass,
            width: Int32(currentConfig.effectiveWidth),
            height: Int32(currentConfig.effectiveHeight),
            soundEnabled: currentConfig.soundEnabled
        )
    }
    
    private func restartEmulation() {
        J2MEBridge.stopEmulator()
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            self.startEmulation()
        }
    }
}