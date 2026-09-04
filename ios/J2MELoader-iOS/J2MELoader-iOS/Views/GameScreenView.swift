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
                // Thanh điều khiển trên cùng (Retro Action Bar)
                HStack(spacing: 12) {
                    Button(action: {
                        gameManager.stopEmulation()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        HStack(spacing: 4) {
                            Image(systemName: "chevron.backward")
                                .font(.system(size: 16, weight: .bold))
                            Text("Thư viện")
                                .font(.system(size: 13, weight: .semibold))
                        }
                        .foregroundColor(.white.opacity(0.9))
                    }
                    
                    VStack(alignment: .leading, spacing: 1) {
                        Text(game.title)
                            .font(.system(size: 14, weight: .bold))
                            .foregroundColor(.white)
                            .lineLimit(1)
                        
                        Text("\(currentConfig.effectiveWidth)x\(currentConfig.effectiveHeight) • \(currentConfig.targetFps * speedMultiplier) FPS")
                            .font(.system(size: 10, weight: .medium))
                            .foregroundColor(.white.opacity(0.6))
                    }
                    
                    Spacer()
                    
                    // Nút tăng tốc (1x, 2x, 4x)
                    Button(action: {
                        if speedMultiplier == 1 { speedMultiplier = 2 }
                        else if speedMultiplier == 2 { speedMultiplier = 4 }
                        else { speedMultiplier = 1 }
                    }) {
                        Text("\(speedMultiplier)x")
                            .font(.system(size: 12, weight: .bold))
                            .padding(.horizontal, 7)
                            .padding(.vertical, 3)
                            .background(speedMultiplier > 1 ? J2MEColors.accent : Color.white.opacity(0.2))
                            .foregroundColor(.white)
                            .cornerRadius(6)
                    }
                    
                    // Nút Khởi động lại game
                    Button(action: { restartEmulation() }) {
                        Image(systemName: "arrow.counterclockwise.circle.fill")
                            .font(.system(size: 22))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Nút Tạm dừng / Tiếp tục
                    Button(action: {
                        isPaused.toggle()
                        J2MEBridge.setPaused(isPaused)
                    }) {
                        Image(systemName: isPaused ? "play.circle.fill" : "pause.circle.fill")
                            .font(.system(size: 24))
                            .foregroundColor(isPaused ? .yellow : .white.opacity(0.85))
                    }
                    
                    // Nút Cài đặt nhanh
                    Button(action: { showingSettings = true }) {
                        Image(systemName: "gearshape.fill")
                            .font(.system(size: 20))
                            .foregroundColor(.white.opacity(0.85))
                    }
                }
                .padding(.horizontal, 14)
                .padding(.vertical, 8)
                .background(Color(red: 0x21/255.0, green: 0x21/255.0, blue: 0x21/255.0))
                
                // Màn hình LCD Canvas hiển thị đồ họa 60 FPS
                ZStack {
                    Color.black
                    
                    MetalView(config: currentConfig) { x, y, action in
                        J2MEBridge.sendTouchEvent(x, y: y, action: action)
                    }
                    .aspectRatio(CGFloat(currentConfig.effectiveWidth) / CGFloat(currentConfig.effectiveHeight), contentMode: .fit)
                    .clipped()
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                
                // Bàn phím ảo khi chơi
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