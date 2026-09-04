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
                Section(header: Text("Device Profile Preset")) {
                    Picker("Device Profile", selection: Binding(
                        get: { game.config.preset },
                        set: { newPreset in
                            game.config.preset = newPreset
                        }
                    )) {
                        ForEach(ResolutionPreset.allCases, id: \.self) { preset in
                            Text(preset.rawValue).tag(preset)
                        }
                    }
                }
                
                Section(header: Text("Display Resolution & Filter")) {
                    if game.config.preset == .custom {
                        HStack {
                            Text("Width")
                            Spacer()
                            TextField("Width", value: $game.config.customWidth, formatter: NumberFormatter())
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                        HStack {
                            Text("Height")
                            Spacer()
                            TextField("Height", value: $game.config.customHeight, formatter: NumberFormatter())
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                        }
                    }
                    
                    Picker("Scaling Mode", selection: $game.config.scalingMode) {
                        ForEach(ScalingMode.allCases, id: \.self) { mode in
                            Text(mode.rawValue).tag(mode)
                        }
                    }
                    
                    Picker("Shader Filter", selection: $game.config.filterMode) {
                        ForEach(FilterMode.allCases, id: \.self) { filter in
                            Text(filter.rawValue).tag(filter)
                        }
                    }
                    
                    Button(action: { showingShaderTune = true }) {
                        Label("Shader Tuning Parameters...", systemImage: "slider.horizontal.3")
                    }
                    
                    Stepper("Target FPS: \(game.config.targetFps)", value: $game.config.targetFps, in: 15...120, step: 5)
                }
                
                Section(header: Text("Controls & Key Mapper")) {
                    Picker("Keypad Layout", selection: $game.config.keypadLayout) {
                        ForEach(KeypadLayout.allCases, id: \.self) { layout in
                            Text(layout.rawValue).tag(layout)
                        }
                    }
                    
                    Button(action: { showingKeyMapper = true }) {
                        Label("Configure Key Mapper (Bluetooth/Gamepad)", systemImage: "gamecontroller")
                    }
                    
                    Toggle("Haptic Feedback (Vibration)", isOn: $game.config.hapticFeedback)
                    Toggle("Direct Touchscreen Canvas", isOn: $game.config.touchScreenEnabled)
                    
                    VStack(alignment: .leading) {
                        Text("Keypad Opacity: \(Int(game.config.keypadOpacity * 100))%")
                        Slider(value: $game.config.keypadOpacity, in: 0.2...1.0, step: 0.05)
                    }
                }
                
                Section(header: Text("Audio & Sonivox EAS MIDI")) {
                    Toggle("Enable Sound & MIDI Synth", isOn: $game.config.soundEnabled)
                    
                    if game.config.soundEnabled {
                        VStack(alignment: .leading) {
                            Text("Volume: \(Int(game.config.soundVolume * 100))%")
                            Slider(value: $game.config.soundVolume, in: 0.0...1.0, step: 0.05)
                        }
                    }
                }
                
                Section(header: Text("MIDlet Info")) {
                    HStack {
                        Text("Name")
                        Spacer()
                        Text(game.title).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Vendor")
                        Spacer()
                        Text(game.vendor).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Version")
                        Spacer()
                        Text(game.version).foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Main Class")
                        Spacer()
                        Text(game.mainClass).foregroundColor(.secondary).font(.footnote)
                    }
                }
            }
            .navigationTitle("Game Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        presentationMode.wrappedValue.dismiss()
                    }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Save") {
                        onSave(game)
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.headline)
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