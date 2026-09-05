import SwiftUI

public struct GameScreenView: View {
    public var game: GameItem
    @ObservedObject var gameManager: GameManager
    
    @State private var isPaused: Bool = false
    @State private var speedMultiplier: Int = 1 // 1x, 2x, 4x
    @State private var showingSettings: Bool = false
    @State private var showingKeyMapper: Bool = false
    @State private var currentConfig: EmulatorConfig
    @Environment(\.presentationMode) var presentationMode
    
    public init(game: GameItem, gameManager: GameManager) {
        self.game = game
        _gameManager = ObservedObject(wrappedValue: gameManager)
        _currentConfig = State(initialValue: game.config)
    }
    
    public var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            
            VStack(spacing: 0) {
                // Thanh điều khiển trên cùng (Retro Action Bar)
                HStack(spacing: 8) {
                    Button(action: {
                        gameManager.stopEmulation()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        HStack(spacing: 3) {
                            Image(systemName: "chevron.backward")
                                .font(.system(size: 11, weight: .bold))
                            Text("Thư viện")
                                .font(.system(size: 12, weight: .semibold))
                        }
                        .foregroundColor(.white.opacity(0.9))
                    }
                    
                    VStack(alignment: .leading, spacing: 1) {
                        Text(game.title)
                            .font(.system(size: 12.5, weight: .bold))
                            .foregroundColor(.white)
                            .lineLimit(1)
                        
                        Text("\(currentConfig.effectiveWidth)x\(currentConfig.effectiveHeight) • \(currentConfig.targetFps * speedMultiplier) FPS")
                            .font(.system(size: 9.5, weight: .medium, design: .monospaced))
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
                            .font(.system(size: 10, weight: .bold))
                            .padding(.horizontal, 5)
                            .padding(.vertical, 2.5)
                            .background(speedMultiplier > 1 ? J2MEColors.accent : Color.white.opacity(0.2))
                            .foregroundColor(.white)
                            .cornerRadius(4)
                    }
                    
                    // Menu chọn nhanh kiểu bàn phím ảo
                    Menu {
                        ForEach(KeypadLayout.allCases, id: \.self) { layout in
                            Button(action: {
                                currentConfig.keypadLayout = layout
                                var updated = game
                                updated.config = currentConfig
                                gameManager.updateGame(updated)
                            }) {
                                HStack {
                                    Text(layout.displayName)
                                    if currentConfig.keypadLayout == layout {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        Image(systemName: "keyboard.fill")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Menu chọn nhanh tỉ lệ màn hình
                    Menu {
                        ForEach(ScalingMode.allCases, id: \.self) { mode in
                            Button(action: {
                                currentConfig.scalingMode = mode
                                var updated = game
                                updated.config = currentConfig
                                gameManager.updateGame(updated)
                            }) {
                                HStack {
                                    Text(mode.displayName)
                                    if currentConfig.scalingMode == mode {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        Image(systemName: "aspectratio.fill")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Menu xoay hướng màn hình nhanh
                    Menu {
                        ForEach(ScreenOrientation.allCases, id: \.self) { orientation in
                            Button(action: {
                                currentConfig.screenOrientation = orientation
                                var updated = game
                                updated.config = currentConfig
                                gameManager.updateGame(updated)
                            }) {
                                HStack {
                                    Text(orientation.displayName)
                                    if currentConfig.screenOrientation == orientation {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        Image(systemName: "arrow.triangle.2.circlepath")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Nút Ánh xạ phím tay cầm
                    Button(action: { showingKeyMapper = true }) {
                        Image(systemName: "gamecontroller.fill")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Nút Chụp ảnh màn hình
                    Button(action: { takeScreenshot() }) {
                        Image(systemName: "camera.fill")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Nút Khởi động lại game
                    Button(action: { restartEmulation() }) {
                        Image(systemName: "arrow.counterclockwise.circle.fill")
                            .font(.system(size: 16))
                            .foregroundColor(.white.opacity(0.85))
                    }
                    
                    // Nút Tạm dừng / Tiếp tục
                    Button(action: {
                        isPaused.toggle()
                        J2MEBridge.setPaused(isPaused)
                    }) {
                        Image(systemName: isPaused ? "play.circle.fill" : "pause.circle.fill")
                            .font(.system(size: 17))
                            .foregroundColor(isPaused ? .yellow : .white.opacity(0.85))
                    }
                    
                    // Nút Cài đặt toàn diện
                    Button(action: { showingSettings = true }) {
                        Image(systemName: "gearshape.fill")
                            .font(.system(size: 15))
                            .foregroundColor(.white.opacity(0.85))
                    }
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 7)
                .background(Color(red: 0x21/255.0, green: 0x21/255.0, blue: 0x21/255.0))
                
                // Màn hình LCD Canvas hiển thị đồ họa 60 FPS
                ZStack {
                    Color(
                        red: Double((currentConfig.screenBgColor.hexColor >> 16) & 0xFF) / 255.0,
                        green: Double((currentConfig.screenBgColor.hexColor >> 8) & 0xFF) / 255.0,
                        blue: Double(currentConfig.screenBgColor.hexColor & 0xFF) / 255.0
                    )
                    
                    MetalView(config: currentConfig) { x, y, action in
                        J2MEBridge.sendTouchEvent(x, y: y, action: action)
                    }
                    .aspectRatio(CGFloat(currentConfig.effectiveWidth) / CGFloat(currentConfig.effectiveHeight), contentMode: currentConfig.scalingMode == .stretch ? .fill : .fit)
                    .clipped()
                    
                    // Badge hiển thị FPS thời gian thực (nếu bật trong cài đặt)
                    if currentConfig.showFps {
                        VStack {
                            HStack {
                                Spacer()
                                Text("\(currentConfig.targetFps * speedMultiplier) FPS")
                                    .font(.system(size: 9.5, weight: .bold, design: .monospaced))
                                    .padding(.horizontal, 5)
                                    .padding(.vertical, 2)
                                    .background(Color.black.opacity(0.65))
                                    .foregroundColor(.green)
                                    .cornerRadius(4)
                                    .padding(8)
                            }
                            Spacer()
                        }
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                
                // Bàn phím ảo khi chơi
                if currentConfig.keypadLayout != .hidden {
                    VirtualKeypadView(config: currentConfig) { keyCode, isDown in
                        J2MEBridge.sendKeyEvent(keyCode, isDown: isDown)
                    }
                    .padding(.bottom, 6)
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
            SettingsView(game: game, onSave: { updated in
                self.currentConfig = updated.config
                gameManager.updateGame(updated)
            })
        }
        .sheet(isPresented: $showingKeyMapper) {
            KeyMapperView()
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
    
    private func takeScreenshot() {
        let w = Int(currentConfig.effectiveWidth)
        let h = Int(currentConfig.effectiveHeight)
        guard let pixelData = J2MEBridge.getFramebufferData() else { return }
        
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue)
        
        guard let provider = CGDataProvider(data: pixelData as CFData),
              let cgImage = CGImage(
                width: w,
                height: h,
                bitsPerComponent: 8,
                bitsPerPixel: 32,
                bytesPerRow: w * 4,
                space: colorSpace,
                bitmapInfo: bitmapInfo,
                provider: provider,
                decode: nil,
                shouldInterpolate: false,
                intent: .defaultIntent
              ) else { return }
        
        let uiImage = UIImage(cgImage: cgImage)
        UIImageWriteToSavedPhotosAlbum(uiImage, nil, nil, nil)
    }
}