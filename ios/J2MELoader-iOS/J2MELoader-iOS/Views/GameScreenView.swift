import SwiftUI

public struct GameScreenView: View {
    public var game: GameItem
    @ObservedObject var gameManager: GameManager
    
    @State private var isPaused: Bool = false
    @State private var speedMultiplier: Int = 1 // 1x, 2x, 4x
    @State private var actualFps: Int = 0
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
                // Thanh điều khiển trên cùng (Modern Compact Action Bar)
                HStack(spacing: 10) {
                    Button(action: {
                        J2MEBridge.setSpeedMultiplier(1)
                        gameManager.stopEmulation()
                        presentationMode.wrappedValue.dismiss()
                    }) {
                        HStack(spacing: 3) {
                            Image(systemName: "chevron.backward")
                                .font(.system(size: 12, weight: .bold))
                            Text("Thư viện")
                                .font(.system(size: 13, weight: .semibold))
                        }
                        .foregroundColor(.white.opacity(0.95))
                    }
                    
                    VStack(alignment: .leading, spacing: 1) {
                        Text(game.title)
                            .font(.system(size: 13, weight: .bold))
                            .foregroundColor(.white)
                            .lineLimit(1)
                        
                        let displayFps = actualFps > 0 ? actualFps : (currentConfig.targetFps * speedMultiplier)
                        Text("\(currentConfig.effectiveWidth)x\(currentConfig.effectiveHeight) • \(displayFps) FPS")
                            .font(.system(size: 9.5, weight: .medium, design: .monospaced))
                            .foregroundColor(.white.opacity(0.65))
                    }
                    
                    Spacer()
                    
                    // Nút tăng tốc (1x, 2x, 4x)
                    Button(action: {
                        if speedMultiplier == 1 { speedMultiplier = 2 }
                        else if speedMultiplier == 2 { speedMultiplier = 4 }
                        else { speedMultiplier = 1 }
                        J2MEBridge.setSpeedMultiplier(Int32(speedMultiplier))
                    }) {
                        Text("\(speedMultiplier)x")
                            .font(.system(size: 11, weight: .bold))
                            .padding(.horizontal, 6)
                            .padding(.vertical, 3)
                            .background(speedMultiplier > 1 ? J2MEColors.accent : Color.white.opacity(0.18))
                            .foregroundColor(.white)
                            .cornerRadius(5)
                    }
                    
                    // Nút Tạm dừng / Tiếp tục
                    Button(action: {
                        isPaused.toggle()
                        J2MEBridge.setPaused(isPaused)
                    }) {
                        Image(systemName: isPaused ? "play.circle.fill" : "pause.circle.fill")
                            .font(.system(size: 18))
                            .foregroundColor(isPaused ? .yellow : .white.opacity(0.9))
                    }
                    
                    // Menu Thao tác Thêm (...)
                    Menu {
                        // Nhóm hiển thị & bàn phím
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
                            Label("Bàn phím ảo", systemImage: "keyboard")
                        }
                        
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
                            Label("Tỉ lệ màn hình", systemImage: "aspectratio")
                        }
                        
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
                            Label("Xoay màn hình", systemImage: "arrow.triangle.2.circlepath")
                        }
                        
                        Divider()
                        
                        // Công cụ
                        Button(action: { showingKeyMapper = true }) {
                            Label("Gán phím tay cầm", systemImage: "gamecontroller")
                        }
                        Button(action: { takeScreenshot() }) {
                            Label("Chụp ảnh màn hình", systemImage: "camera")
                        }
                        Button(action: { restartEmulation() }) {
                            Label("Khởi động lại game", systemImage: "arrow.counterclockwise")
                        }
                        
                        Divider()
                        
                        Button(action: { showingSettings = true }) {
                            Label("Cài đặt chi tiết", systemImage: "gearshape")
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle.fill")
                            .font(.system(size: 19))
                            .foregroundColor(.white.opacity(0.9))
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(Color(red: 0x1e/255.0, green: 0x1e/255.0, blue: 0x1e/255.0))
                
                // Màn hình LCD Canvas hiển thị đồ họa 60 FPS
                ZStack {
                    Color(
                        red: Double((currentConfig.screenBgColor.hexColor >> 16) & 0xFF) / 255.0,
                        green: Double((currentConfig.screenBgColor.hexColor >> 8) & 0xFF) / 255.0,
                        blue: Double(currentConfig.screenBgColor.hexColor & 0xFF) / 255.0
                    )
                    
                    MetalView(
                        config: currentConfig,
                        speedMultiplier: speedMultiplier,
                        onFpsUpdate: { fps in
                            self.actualFps = fps
                        }
                    ) { x, y, action in
                        J2MEBridge.sendTouchEvent(x, y: y, action: action)
                    }
                    .aspectRatio(CGFloat(currentConfig.effectiveWidth) / CGFloat(currentConfig.effectiveHeight), contentMode: currentConfig.scalingMode == .stretch ? .fill : .fit)
                    .clipped()
                    
                    // Badge hiển thị FPS thời gian thực (nếu bật trong cài đặt)
                    if currentConfig.showFps {
                        let displayFps = actualFps > 0 ? actualFps : (currentConfig.targetFps * speedMultiplier)
                        let fpsColor: Color = displayFps >= 50 ? .green : (displayFps >= 30 ? .yellow : .orange)
                        VStack {
                            HStack {
                                Spacer()
                                Text("\(displayFps) FPS")
                                    .font(.system(size: 9.5, weight: .bold, design: .monospaced))
                                    .padding(.horizontal, 5)
                                    .padding(.vertical, 2)
                                    .background(Color.black.opacity(0.65))
                                    .foregroundColor(fpsColor)
                                    .cornerRadius(4)
                                    .padding(8)
                            }
                            Spacer()
                        }
                    }
                    
                    // Overlay bàn phím cảm ứng nổi (nếu chọn touchOverlay)
                    if currentConfig.keypadLayout == .touchOverlay {
                        VStack {
                            Spacer()
                            VirtualKeypadView(config: currentConfig) { keyCode, isDown in
                                J2MEBridge.sendKeyEvent(keyCode, isDown: isDown)
                            }
                            .padding(.bottom, 8)
                        }
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                
                // Bàn phím ảo khi chơi (khi không phải hidden và không phải touchOverlay)
                if currentConfig.keypadLayout != .hidden && currentConfig.keypadLayout != .touchOverlay {
                    VirtualKeypadView(config: currentConfig) { keyCode, isDown in
                        J2MEBridge.sendKeyEvent(keyCode, isDown: isDown)
                    }
                    .padding(.bottom, 6)
                }
            }
        }
        .onAppear {
            startEmulation()
            GamePadManager.shared.startMonitoring()
        }
        .onDisappear {
            GamePadManager.shared.stopMonitoring()
            J2MEBridge.setSpeedMultiplier(1)
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