import SwiftUI

public struct ShaderTuneView: View {
    @Binding public var config: EmulatorConfig
    @Environment(\.presentationMode) var presentationMode
    
    public init(config: Binding<EmulatorConfig>) {
        self._config = config
    }
    
    public var body: some View {
        NavigationView {
            Form {
                Section(header: Text("TINH CHỈNH HIỆU ỨNG SHADER METAL").font(.system(size: 11.5, weight: .semibold))) {
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ sáng màn hình (Brightness)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text(String(format: "%.2f", config.shaderBrightness))
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $config.shaderBrightness, in: -0.5...0.5, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ tương phản (Contrast)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text(String(format: "%.2f", config.shaderContrast))
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $config.shaderContrast, in: 0.5...2.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Độ đậm quét sọc TV CRT (Scanlines)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text("\(Int(config.shaderScanlineIntensity * 100))%")
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $config.shaderScanlineIntensity, in: 0.0...1.0, step: 0.05)
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text("Lưới điểm ảnh Nokia LCD (Grid Strength)")
                                .font(.system(size: 13, weight: .regular))
                            Spacer()
                            Text("\(Int(config.shaderLcdGridStrength * 100))%")
                                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                .foregroundColor(.secondary)
                        }
                        Slider(value: $config.shaderLcdGridStrength, in: 0.0...1.0, step: 0.05)
                    }
                }
                
                Section {
                    Button("Khôi phục mặc định", role: .destructive) {
                        config.shaderBrightness = 0.0
                        config.shaderContrast = 1.0
                        config.shaderScanlineIntensity = 0.25
                        config.shaderLcdGridStrength = 0.20
                    }
                    .font(.system(size: 13.5, weight: .regular))
                }
            }
            .navigationTitle("Hiệu ứng Shader")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Xong") {
                        presentationMode.wrappedValue.dismiss()
                    }
                    .font(.system(size: 14, weight: .bold))
                    .foregroundColor(J2MEColors.accent)
                }
            }
        }
        .navigationViewStyle(StackNavigationViewStyle())
    }
}