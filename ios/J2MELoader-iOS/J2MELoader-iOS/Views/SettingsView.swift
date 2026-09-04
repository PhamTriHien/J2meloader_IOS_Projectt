import SwiftUI

public struct SettingsView: View {
    @State public var game: GameItem
    public var onSave: (GameItem) -> Void
    @Environment(\.presentationMode) var presentationMode
    
    @State private var showingKeyMapper = false
    @State private var showingShaderTune = false
    
    public init(game: GameItem, onSave: @escaping (GameItem) -> Void) {
        _game = State(initialValue: game)
        self.onSave = onSave
    }
    
    public var body: some View {
        NavigationView {
            Form {
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