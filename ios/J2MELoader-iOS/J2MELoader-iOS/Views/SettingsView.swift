import SwiftUI

public enum SettingsSubSheet: Identifiable {
    case keyMapper
    case shaderTune
    
    public var id: String {
        switch self {
        case .keyMapper: return "keyMapper"
        case .shaderTune: return "shaderTune"
        }
    }
}

public struct SettingsView: View {
    @State public var game: GameItem
    public var onSave: (GameItem) -> Void
    public var onStart: ((GameItem) -> Void)?
    @Environment(\.presentationMode) var presentationMode
    
    @State private var subSheet: SettingsSubSheet? = nil
    
    public init(game: GameItem, onSave: @escaping (GameItem) -> Void, onStart: ((GameItem) -> Void)? = nil) {
        _game = State(initialValue: game)
        self.onSave = onSave
        self.onStart = onStart
    }
    
    public var body: some View {
        NavigationView {
            Form {
                // Header Game Card & Nút Bắt đầu chơi (START)
                Section {
                    VStack(spacing: 12) {
                        HStack(spacing: 12) {
                            ZStack {
                                Color(red: 0x52/255.0, green: 0x5a/255.0, blue: 0xa0/255.0)
                                Image(systemName: "gamecontroller.fill")
                                    .font(.system(size: 22))
                                    .foregroundColor(.white)
                            }
                            .frame(width: 44, height: 44)
                            .cornerRadius(8)
                            
                            VStack(alignment: .leading, spacing: 3) {
                                Text(game.title)
                                    .font(.system(size: 15, weight: .bold))
                                    .foregroundColor(.primary)
                                    .lineLimit(1)
                                
                                Text("\(game.vendor) • v\(game.version)")
                                    .font(.system(size: 12, weight: .regular))
                                    .foregroundColor(.secondary)
                            }
                            Spacer()
                        }
                        
                        if let startAction = onStart {
                            Button(action: {
                                onSave(game)
                                presentationMode.wrappedValue.dismiss()
                                startAction(game)
                            }) {
                                HStack(spacing: 6) {
                                    Image(systemName: "play.fill")
                                        .font(.system(size: 14, weight: .bold))
                                    Text("BẮT ĐẦU CHƠI")
                                        .font(.system(size: 14.5, weight: .bold))
                                }
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 11)
                                .background(J2MEColors.accent)
                                .cornerRadius(8)
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                    }
                    .padding(.vertical, 3)
                }
                
                Section(header: Text("THIẾT BỊ MẪU (DEVICE PROFILE)").font(.system(size: 11.5, weight: .semibold))) {
                    Picker(selection: Binding(
                        get: { game.config.preset },
                        set: { newPreset in
                            game.config.preset = newPreset
                        }
                    )) {
                        ForEach(ResolutionPreset.allCases, id: \.self) { preset in
                            Text(preset.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(preset)
                        }
                    } label: {
                        Text("Hồ sơ thiết bị")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                }
                
                Section(header: Text("ĐỘ PHÂN GIẢI & BỘ LỌC ĐỒ HỌA").font(.system(size: 11.5, weight: .semibold))) {
                    if game.config.preset == .custom {
                        HStack {
                            Text("Chiều rộng (Width)")
                                .font(.system(size: 13.5, weight: .regular))
                            Spacer()
                            TextField("Rộng", value: $game.config.customWidth, formatter: NumberFormatter())
                                .font(.system(size: 13, weight: .regular))
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                        HStack {
                            Text("Chiều cao (Height)")
                                .font(.system(size: 13.5, weight: .regular))
                            Spacer()
                            TextField("Cao", value: $game.config.customHeight, formatter: NumberFormatter())
                                .font(.system(size: 13, weight: .regular))
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                    }
                    
                    Picker(selection: $game.config.screenOrientation) {
                        ForEach(ScreenOrientation.allCases, id: \.self) { orientation in
                            Text(orientation.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(orientation)
                        }
                    } label: {
                        Text("Hướng xoay màn hình")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Picker(selection: $game.config.screenBgColor) {
                        ForEach(ScreenBgColor.allCases, id: \.self) { bg in
                            Text(bg.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(bg)
                        }
                    } label: {
                        Text("Màu nền màn hình LCD")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Picker(selection: $game.config.scalingMode) {
                        ForEach(ScalingMode.allCases, id: \.self) { mode in
                            Text(mode.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(mode)
                        }
                    } label: {
                        Text("Chế độ co giãn")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Picker(selection: $game.config.filterMode) {
                        ForEach(FilterMode.allCases, id: \.self) { filter in
                            Text(filter.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(filter)
                        }
                    } label: {
                        Text("Bộ lọc Shader Metal")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Button(action: { subSheet = .shaderTune }) {
                        HStack {
                            Image(systemName: "slider.horizontal.3")
                                .font(.system(size: 13.5))
                            Text("Tinh chỉnh thông số Shader...")
                                .font(.system(size: 13.5, weight: .medium))
                        }
                        .foregroundColor(J2MEColors.accent)
                    }
                    
                    Toggle(isOn: $game.config.showFps) {
                        Text("Hiển thị chỉ số đo FPS thời gian thực")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Stepper(value: $game.config.targetFps, in: 15...120, step: 5) {
                        Text("Giới hạn khung hình: \(game.config.targetFps) FPS")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                }
                
                Section(header: Text("ĐIỀU KHIỂN & BÀN PHÍM ẢO").font(.system(size: 11.5, weight: .semibold))) {
                    Picker(selection: $game.config.keypadLayout) {
                        ForEach(KeypadLayout.allCases, id: \.self) { layout in
                            Text(layout.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(layout)
                        }
                    } label: {
                        Text("Kiểu bàn phím")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Button(action: { subSheet = .keyMapper }) {
                        HStack {
                            Image(systemName: "gamecontroller")
                                .font(.system(size: 13.5))
                            Text("Cài đặt gán phím tay cầm (Bluetooth / MFi)")
                                .font(.system(size: 13.5, weight: .medium))
                        }
                        .foregroundColor(J2MEColors.accent)
                    }
                    
                    Toggle(isOn: $game.config.hapticFeedback) {
                        Text("Rung phản hồi khi bấm phím (Haptic)")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    Toggle(isOn: $game.config.touchScreenEnabled) {
                        Text("Cho phép chạm cảm ứng trên màn hình")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Độ trong suốt bàn phím: \(Int(game.config.keypadOpacity * 100))%")
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                        Slider(value: $game.config.keypadOpacity, in: 0.2...1.0, step: 0.05)
                    }
                }
                
                Section(header: Text("ÂM THANH & BỘ TỔNG HỢP SONIVOX EAS").font(.system(size: 11.5, weight: .semibold))) {
                    Toggle(isOn: $game.config.soundEnabled) {
                        Text("Bật âm thanh & Nhạc chuông MIDI")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    if game.config.soundEnabled {
                        VStack(alignment: .leading, spacing: 4) {
                            Text("Âm lượng: \(Int(game.config.soundVolume * 100))%")
                                .font(.system(size: 12.5, weight: .regular))
                                .foregroundColor(.secondary)
                            Slider(value: $game.config.soundVolume, in: 0.0...1.0, step: 0.05)
                        }
                    }
                }
                
                Section(header: Text("TƯƠNG THÍCH HỆ THỐNG & FONT CHỮ").font(.system(size: 11.5, weight: .semibold))) {
                    Picker(selection: $game.config.fontSizeScale) {
                        ForEach(FontSizeScale.allCases, id: \.self) { scale in
                            Text(scale.displayName)
                                .font(.system(size: 13, weight: .regular))
                                .tag(scale)
                        }
                    } label: {
                        Text("Kích cỡ chữ MIDP (Font Scale)")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    
                    HStack {
                        Text("Mã thiết bị giả lập (Platform)")
                            .font(.system(size: 13.5, weight: .regular))
                        Spacer()
                        TextField("NokiaN73", text: $game.config.systemPlatform)
                            .font(.system(size: 13, weight: .regular))
                            .multilineTextAlignment(.trailing)
                    }
                    
                    HStack {
                        Text("Ngôn ngữ (Locale)")
                            .font(.system(size: 13.5, weight: .regular))
                        Spacer()
                        TextField("vi-VN", text: $game.config.systemLocale)
                            .font(.system(size: 13, weight: .regular))
                            .multilineTextAlignment(.trailing)
                    }
                }
                
                Section(header: Text("TỐI ƯU HIỆU NĂNG & TREO NGẦM (ANTI-CRASH)").font(.system(size: 11.5, weight: .semibold))) {
                    Toggle(isOn: $game.config.backgroundKeepAlive) {
                        Text("Treo ngầm 24/7 (Không ngắt kết nối / Không bị văng)")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    Toggle(isOn: $game.config.networkKeepAlive) {
                        Text("Giữ kết nối mạng Socket liên tục (Anti-Disconnect)")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                    Toggle(isOn: $game.config.vsyncMetal) {
                        Text("Đồng bộ khung hình VSync Metal 60 FPS")
                            .font(.system(size: 13.5, weight: .regular))
                    }
                }
                
                Section(header: Text("THÔNG TIN GAME MIDLET").font(.system(size: 11.5, weight: .semibold))) {
                    HStack {
                        Text("Tên game")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text(game.title)
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Nhà sản xuất")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text(game.vendor)
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Phiên bản")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text(game.version)
                            .font(.system(size: 12.5, weight: .regular))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Lớp khởi chạy (Main Class)")
                            .font(.system(size: 13, weight: .regular))
                        Spacer()
                        Text(game.mainClass)
                            .font(.system(size: 11.5, weight: .regular, design: .monospaced))
                            .foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("Cài đặt game")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Hủy") {
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.system(size: 14, weight: .regular))
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Lưu") {
                        onSave(game)
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.system(size: 14, weight: .bold))
                    .foregroundColor(J2MEColors.accent)
                }
            }
            .sheet(item: $subSheet) { sheet in
                switch sheet {
                case .keyMapper:
                    KeyMapperView()
                case .shaderTune:
                    ShaderTuneView()
                }
            }
        }
    }
}