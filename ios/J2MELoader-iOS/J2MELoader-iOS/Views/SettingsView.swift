import SwiftUI

public struct SettingsView: View {
    @State public var game: GameItem
    public var onSave: (GameItem) -> Void
    public var onStart: ((GameItem) -> Void)?
    @Environment(\.presentationMode) var presentationMode
    
    @State private var showingKeyMapper = false
    @State private var showingShaderTune = false
    
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
                                    .font(.system(size: 24))
                                    .foregroundColor(.white)
                            }
                            .frame(width: 48, height: 48)
                            .cornerRadius(8)
                            
                            VStack(alignment: .leading, spacing: 3) {
                                Text(game.title)
                                    .font(.system(size: 17, weight: .bold))
                                    .foregroundColor(.primary)
                                    .lineLimit(1)
                                
                                Text("\(game.vendor) • v\(game.version)")
                                    .font(.system(size: 13))
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
                                HStack {
                                    Image(systemName: "play.fill")
                                        .font(.system(size: 16, weight: .bold))
                                    Text("BẮT ĐẦU CHƠI")
                                        .font(.system(size: 16, weight: .bold))
                                }
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 12)
                                .background(J2MEColors.accent)
                                .cornerRadius(8)
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                    }
                    .padding(.vertical, 4)
                }
                
                Section(header: Text("Thiết bị mẫu (Device Profile)")) {
                    Picker("Hồ sơ thiết bị", selection: Binding(
                        get: { game.config.preset },
                        set: { newPreset in
                            game.config.preset = newPreset
                        }
                    )) {
                        ForEach(ResolutionPreset.allCases, id: \.self) { preset in
                            Text(preset.displayName).tag(preset)
                        }
                    }
                }
                
                Section(header: Text("Độ phân giải & Bộ lọc đồ họa")) {
                    if game.config.preset == .custom {
                        HStack {
                            Text("Chiều rộng (Width)")
                            Spacer()
                            TextField("Rộng", value: $game.config.customWidth, formatter: NumberFormatter())
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                        HStack {
                            Text("Chiều cao (Height)")
                            Spacer()
                            TextField("Cao", value: $game.config.customHeight, formatter: NumberFormatter())
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                    }
                    
                    Picker("Hướng xoay màn hình", selection: $game.config.screenOrientation) {
                        ForEach(ScreenOrientation.allCases, id: \.self) { orientation in
                            Text(orientation.displayName).tag(orientation)
                        }
                    }
                    
                    Picker("Màu nền màn hình LCD", selection: $game.config.screenBgColor) {
                        ForEach(ScreenBgColor.allCases, id: \.self) { bg in
                            Text(bg.displayName).tag(bg)
                        }
                    }
                    
                    Picker("Chế độ co giãn", selection: $game.config.scalingMode) {
                        ForEach(ScalingMode.allCases, id: \.self) { mode in
                            Text(mode.displayName).tag(mode)
                        }
                    }
                    
                    Picker("Bộ lọc Shader", selection: $game.config.filterMode) {
                        ForEach(FilterMode.allCases, id: \.self) { filter in
                            Text(filter.displayName).tag(filter)
                        }
                    }
                    
                    Button(action: { showingShaderTune = true }) {
                        Label("Tinh chỉnh thông số Shader...", systemImage: "slider.horizontal.3")
                    }
                    
                    Toggle("Hiển thị chỉ số đo FPS thời gian thực", isOn: $game.config.showFps)
                    Stepper("Giới hạn khung hình: \(game.config.targetFps) FPS", value: $game.config.targetFps, in: 15...120, step: 5)
                }
                
                Section(header: Text("Điều khiển & Bàn phím ảo")) {
                    Picker("Kiểu bàn phím", selection: $game.config.keypadLayout) {
                        ForEach(KeypadLayout.allCases, id: \.self) { layout in
                            Text(layout.displayName).tag(layout)
                        }
                    }
                    
                    Button(action: { showingKeyMapper = true }) {
                        Label("Cài đặt gán phím tay cầm (Bluetooth / MFi)", systemImage: "gamecontroller")
                    }
                    
                    Toggle("Rung phản hồi khi bấm phím (Haptic)", isOn: $game.config.hapticFeedback)
                    Toggle("Cho phép chạm cảm ứng trên màn hình", isOn: $game.config.touchScreenEnabled)
                    
                    VStack(alignment: .leading) {
                        Text("Độ trong suốt bàn phím: \(Int(game.config.keypadOpacity * 100))%")
                        Slider(value: $game.config.keypadOpacity, in: 0.2...1.0, step: 0.05)
                    }
                }
                
                Section(header: Text("Âm thanh & Bộ tổng hợp Sonivox EAS")) {
                    Toggle("Bật âm thanh & Nhạc chuông MIDI", isOn: $game.config.soundEnabled)
                    
                    if game.config.soundEnabled {
                        VStack(alignment: .leading) {
                            Text("Âm lượng: \(Int(game.config.soundVolume * 100))%")
                            Slider(value: $game.config.soundVolume, in: 0.0...1.0, step: 0.05)
                        }
                    }
                }
                
                Section(header: Text("Tương thích hệ thống & Chữ (Fonts)")) {
                    Picker("Kích cỡ chữ MIDP (Font Scale)", selection: $game.config.fontSizeScale) {
                        ForEach(FontSizeScale.allCases, id: \.self) { scale in
                            Text(scale.displayName).tag(scale)
                        }
                    }
                    
                    HStack {
                        Text("Mã thiết bị giả lập (Platform)")
                        Spacer()
                        TextField("NokiaN73", text: $game.config.systemPlatform)
                            .multilineTextAlignment(.trailing)
                    }
                    
                    HStack {
                        Text("Ngôn ngữ (Locale)")
                        Spacer()
                        TextField("vi-VN", text: $game.config.systemLocale)
                            .multilineTextAlignment(.trailing)
                    }
                }
                
                Section(header: Text("Tối ưu hiệu năng & Chạy ngầm (Anti-Crash)")) {
                    Toggle("Treo ngầm 24/7 (Không ngắt kết nối / Không bị văng)", isOn: $game.config.backgroundKeepAlive)
                    Toggle("Giữ kết nối mạng Socket liên tục (Anti-Disconnect)", isOn: $game.config.networkKeepAlive)
                    Toggle("Đồng bộ khung hình VSync Metal 60 FPS", isOn: $game.config.vsyncMetal)
                }
                
                Section(header: Text("Thông tin game MIDlet")) {
                    HStack {
                        Text("Tên game")
                        Spacer()
                        Text(game.title).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Nhà sản xuất")
                        Spacer()
                        Text(game.vendor).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Phiên bản")
                        Spacer()
                        Text(game.version).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Lớp khởi chạy (Main Class)")
                        Spacer()
                        Text(game.mainClass).foregroundColor(.secondary).font(.footnote)
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
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Lưu") {
                        onSave(game)
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.headline)
                    .foregroundColor(J2MEColors.accent)
                }
            }
            .sheet(isPresented: $showingKeyMapper) {
                KeyMapperView()
            }
            .sheet(isPresented: $showingShaderTune) {
                ShaderTuneView()
            }
        }
    }
}